#ifdef _WIN32
// Define these before including Windows.h to avoid name conflicts with Raylib
#define NOGDI             // Excludes GDI defines (Rectangle)
#define NOUSER            // Excludes USER defines (ShowCursor)
#define WINDOWS_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h> // Doit être inclus avant windows.h
#include <windows.h>
#else
#include <time.h>
#endif

#include <math.h>

#define NOB_IMPLEMENTATION
#include "../nob.h"

#include "network.h"
#include "world_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NETWORK_IMPL
#include "rnet.h"

// Statistiques du serveur
typedef struct
{
    int packetsReceived;
    int connectPackets;
    int disconnectPackets;
    int statePackets;
    int worldStatePackets;
    int activeConnections;
} ServerStats;

static ServerStats stats = {0};
static double lastDebugPrint = 0;

// Fonction portable pour obtenir le temps en secondes
// Renamed to ServerGetTime to avoid conflict with Raylib's GetTime
static double ServerGetTime(void)
{
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    static int initialized = 0;

    if (!initialized)
    {
        QueryPerformanceFrequency(&frequency);
        initialized = 1;
    }

    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    return (double)currentTime.QuadPart / frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
#endif
}

// Trouver la hauteur du terrain à une position donnée
static int getHeightAt(WorldManager *wm, int x, int z)
{
    int chunkX = floor((float)x / CHUNK_SIZE);
    int chunkZ = floor((float)z / CHUNK_SIZE);
    int localX = x - chunkX * CHUNK_SIZE;
    int localZ = z - chunkZ * CHUNK_SIZE;

    if (localX < 0) localX += CHUNK_SIZE;
    if (localZ < 0) localZ += CHUNK_SIZE;

    ChunkData *chunk = worldManager_getChunk(wm, chunkX, chunkZ);
    if (!chunk)
        return 64; // Default height if chunk not loaded

    // Check from top to bottom for the first non-air block
    for (int y = WORLD_HEIGHT - 1; y >= 0; y--) {
        int section = y / CHUNK_SIZE;
        int localY = y % CHUNK_SIZE;
        
        // Si cette section n'est pas allouée, c'est de l'air
        if (chunk->verticals[section] == NULL) {
            continue; // Skip, it's all air
        }
        
        // Si cette section est compressée (type uniforme)
        if (chunk->verticals[section]->compressed) {
            BlockType blockType = chunk->verticals[section]->uniformBlockType;
            if (blockType != BLOCK_AIR) {
                // Return the top block of this uniform section
                return y;
            }
        } else {
            // Check the specific block in the uncompressed section
            if (chunk->verticals[section]->blocks[localY][localX][localZ].Type != BLOCK_AIR) {
                return y;
            }
        }
    }

    return 0; // No block found
}

NetworkPlayer players[MAX_PLAYERS] = {0};
int nextPlayerId = 1;

WorldManager *world = NULL;

void handleNewConnection(rnetPeer *peer)
{
    rnetTargetPeer *target = rnetGetLastEventPeer(peer);
    if (!target)
        return;

    stats.connectPackets++;
    stats.activeConnections++;
    int id = nextPlayerId++;
    printf("New player connected! Assigned ID: %d\n", id);

    // Trouver un slot libre
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!players[i].connected)
        {
            players[i].connected = true;
            players[i].id = id;

            // Trouver la hauteur du terrain au point de spawn
            int spawnHeight = getHeightAt(world, 0, 0);
            players[i].position = (Vec3){0, spawnHeight, 0};
            players[i].velocity = (Vec3){0, 0, 0};
            players[i].yaw = 0;
            players[i].pitch = 0;

            // Envoyer l'ID au client qui vient de se connecter
            Packet packet = {
                .type = PACKET_CONNECT,
                .player = players[i]};
            rnetSendToPeer(peer, target, &packet, sizeof(Packet), RNET_RELIABLE);
            break;
        }
    }
}

void handlePlayerState(rnetPeer *peer, NetworkPlayer *player)
{
    stats.statePackets++;
    // Mettre à jour l'état du joueur
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (players[i].id == player->id)
        {
            players[i] = *player;
            break;
        }
    }

    // Diffuser la mise à jour à tous les autres clients
    Packet packet = {
        .type = PACKET_PLAYER_STATE,
        .player = *player};

    rnetBroadcast(peer, &packet, sizeof(Packet), RNET_UNRELIABLE);
}

void handleDisconnect(rnetPeer *peer, int playerId)
{
    stats.disconnectPackets++;
    stats.activeConnections--;
    printf("Player %d disconnected\n", playerId);

    // Marquer le joueur comme déconnecté
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (players[i].id == playerId)
        {
            players[i].connected = false;

            // Informer les autres clients
            Packet packet = {
                .type = PACKET_DISCONNECT,
                .player = players[i]};
            rnetBroadcast(peer, &packet, sizeof(Packet), RNET_RELIABLE);
            break;
        }
    }
}

void handleChunkRequest(rnetPeer *peer, int chunkX, int chunkZ)
{
    ChunkData *chunk = worldManager_getChunk(world, chunkX, chunkZ);
    if (!chunk)
        return;

    Packet packet = {
        .type = PACKET_CHUNK_DATA,
        .chunk = *chunk};

    rnetSendToPeer(peer, rnetGetLastEventPeer(peer), &packet, sizeof(Packet), RNET_RELIABLE);
}

void handleBlockUpdate(rnetPeer *peer, BlockUpdate *update)
{
    // Modifier le block dans le monde
    if (worldManager_setBlock(world, update->blockpos.x, update->blockpos.y, update->blockpos.z, update->block))
    {
        // Diffuser la mise à jour à tous les joueurs
        Packet packet = {
            .type = PACKET_BLOCK_UPDATE,
            .blockUpdate = *update};
        rnetBroadcast(peer, &packet, sizeof(Packet), RNET_RELIABLE);
    }
}

void sendBlockUpdateToPlayer(rnetPeer *peer, BlockUpdate *update) 
{
    Packet packet = {
        .type = PACKET_BLOCK_UPDATE,
        .blockUpdate = *update};
    rnetSendToPeer(peer, rnetGetLastEventPeer(peer), &packet, sizeof(Packet), RNET_RELIABLE);
}

int main(void)
{
    printf("Starting Minecraft server on port %d...\n", SERVER_PORT);

    // Initialiser le serveur
    if (!rnetInit())
    {
        printf("Failed to initialize network\n");
        return 1;
    }

    rnetPeer *server = rnetHost(SERVER_PORT);
    if (!server)
    {
        printf("Failed to start server\n");
        rnetShutdown();
        return 1;
    }

    // Initialiser le monde avec une seed aléatoire
    srand(time(NULL));
    world = worldManager_create(rand());
    printf("Server started successfully with seed: %d\n", world->seed);

    // Pré-charger les chunks autour du spawn
    printf("Pre-loading chunks around spawn...\n");
    for (int x = -CHUNK_LOAD_DISTANCE; x <= CHUNK_LOAD_DISTANCE; x++)
    {
        for (int z = -CHUNK_LOAD_DISTANCE; z <= CHUNK_LOAD_DISTANCE; z++)
        {
            worldManager_getChunk(world, x, z);
        }
    }
    printf("Chunks pre-loaded!\n");

    double lastTick = ServerGetTime();  // Changed to ServerGetTime
    lastDebugPrint = ServerGetTime();   // Changed to ServerGetTime

    // Boucle principale du serveur
    while (1)
    {
        double currentTime = ServerGetTime();  // Changed to ServerGetTime

        // Afficher les statistiques périodiquement
        if (currentTime - lastDebugPrint >= SERVER_PRINT_DEBUG_DELAY / 1000.0)
        {
            printf("\n=== Server Stats ===\n");
            printf("Active connections: %d\n", stats.activeConnections);
            printf("Total packets received: %d\n", stats.packetsReceived);
            printf("Connect packets: %d\n", stats.connectPackets);
            printf("Disconnect packets: %d\n", stats.disconnectPackets);
            printf("State update packets: %d\n", stats.statePackets);
            printf("World state packets: %d\n", stats.worldStatePackets);
            printf("==================\n\n");
            lastDebugPrint = currentTime;
        }

        // Mise à jour du serveur à intervalle fixe
        if (currentTime - lastTick >= 1.0 / SERVER_TICK_RATE)
        {
            rnetPacket packet;
            while (rnetReceive(server, &packet))
            {
                if (packet.data == NULL)
                {
                    // C'est un événement de connexion
                    handleNewConnection(server);
                    continue;
                }

                stats.packetsReceived++;
                Packet *gamePacket = (Packet *)packet.data;

                switch (gamePacket->type)
                {
                case PACKET_CONNECT:
                    handleNewConnection(server);
                    break;
                case PACKET_DISCONNECT:
                    handleDisconnect(server, gamePacket->player.id);
                    break;
                case PACKET_PLAYER_STATE:
                    handlePlayerState(server, &gamePacket->player);
                    break;
                case PACKET_CHUNK_REQUEST:
                    handleChunkRequest(server, gamePacket->chunkX, gamePacket->chunkZ);
                    break;
                case PACKET_BLOCK_UPDATE:
                    handleBlockUpdate(server, &gamePacket->blockUpdate);
                    break;
                case PACKET_WORLD_STATE:
                    stats.worldStatePackets++;
                    break;
                default:
                    break;
                }

                rnetFreePacket(&packet);
            }

            lastTick = currentTime;
        }

        // Sauvegarder périodiquement le monde (toutes les 30 secondes)
        static double lastSave = 0;
        if (currentTime - lastSave >= 30.0)
        {
            worldManager_saveAll(world);
            lastSave = currentTime;
        }
    }

    worldManager_destroy(world);
    rnetClose(server);
    rnetShutdown();
    return 0;
}
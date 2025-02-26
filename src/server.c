#ifdef _WIN32
#define WINDOWS_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>  // Doit être inclus avant windows.h
#include <windows.h>
#else
#include <time.h>
#endif

#include "network.h"
#include <stdio.h>
#include <stdlib.h>

#define NETWORK_IMPL
#include "rnet.h"

// Statistiques du serveur
typedef struct {
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
static double GetTime(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    static int initialized = 0;
    
    if (!initialized) {
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

NetworkPlayer players[MAX_PLAYERS] = {0};
int nextPlayerId = 1;

void handleNewConnection(rnetPeer* peer) {
    rnetTargetPeer* target = rnetGetLastEventPeer(peer);
    if (!target) return;
    
    stats.connectPackets++;
    stats.activeConnections++;
    int id = nextPlayerId++;
    printf("New player connected! Assigned ID: %d\n", id);
    
    // Trouver un slot libre
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].connected) {
            players[i].connected = true;
            players[i].id = id;
            players[i].position = (Vec3){0, 65, 0};
            players[i].velocity = (Vec3){0, 0, 0};
            players[i].yaw = 0;
            players[i].pitch = 0;
            
            // Envoyer l'ID au client qui vient de se connecter
            Packet packet = {
                .type = PACKET_CONNECT,
                .player = players[i]
            };
            rnetSendToPeer(peer, target, &packet, sizeof(Packet), RNET_RELIABLE);
            break;
        }
    }
}

void handlePlayerState(rnetPeer* peer, NetworkPlayer* player) {
    stats.statePackets++;
    // Mettre à jour l'état du joueur
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].id == player->id) {
            players[i] = *player;
            break;
        }
    }
    
    // Diffuser la mise à jour à tous les autres clients
    Packet packet = {
        .type = PACKET_PLAYER_STATE,
        .player = *player
    };
    
    rnetBroadcast(peer, &packet, sizeof(Packet), RNET_UNRELIABLE);
}

void handleDisconnect(rnetPeer* peer, int playerId) {
    stats.disconnectPackets++;
    stats.activeConnections--;
    printf("Player %d disconnected\n", playerId);
    
    // Marquer le joueur comme déconnecté
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].id == playerId) {
            players[i].connected = false;
            
            // Informer les autres clients
            Packet packet = {
                .type = PACKET_DISCONNECT,
                .player = players[i]
            };
            rnetBroadcast(peer, &packet, sizeof(Packet), RNET_RELIABLE);
            break;
        }
    }
}

int main(void) {
    printf("Starting Minecraft server on port %d...\n", SERVER_PORT);
    
    // Initialiser le serveur
    if (!rnetInit()) {
        printf("Failed to initialize network\n");
        return 1;
    }
    
    rnetPeer* server = rnetHost(SERVER_PORT);
    if (!server) {
        printf("Failed to start server\n");
        rnetShutdown();
        return 1;
    }
    
    printf("Server started successfully\n");

    double lastTick = GetTime();
    lastDebugPrint = GetTime();
    
    // Boucle principale du serveur
    while (1) {
        double currentTime = GetTime();
        
        // Afficher les statistiques périodiquement
        if (currentTime - lastDebugPrint >= SERVER_PRINT_DEBUG_DELAY / 1000.0) {
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
        if (currentTime - lastTick >= 1.0/SERVER_TICK_RATE) {
            rnetPacket packet;
            while (rnetReceive(server, &packet)) {
                if (packet.data == NULL) {
                    // C'est un événement de connexion
                    handleNewConnection(server);
                    continue;
                }
                
                stats.packetsReceived++;
                Packet* gamePacket = (Packet*)packet.data;
                
                switch (gamePacket->type) {
                    case PACKET_CONNECT:
                        handleNewConnection(server);
                        break;
                    case PACKET_DISCONNECT:
                        handleDisconnect(server, gamePacket->player.id);
                        break;
                    case PACKET_PLAYER_STATE:
                        handlePlayerState(server, &gamePacket->player);
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
    }
    
    rnetClose(server);
    rnetShutdown();
    return 0;
}
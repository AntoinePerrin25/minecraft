#ifdef _WIN32
#define WINDOWS_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#endif

// Inclure raylib avant les autres headers pour éviter les conflits
#include "raylib.h"
#include "renderer.h"
#include "raymath.h"
#include "rlgl.h"

// Autres includes après raylib
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "chunk_mesh.h"

// Définir NETWORK_IMPL après tous les autres includes
#define NETWORK_IMPL
#include "rnet.h"

#define CHUNK_SIZE 16
#define WORLD_HEIGHT 256
#define RENDER_DISTANCE 8

#define WINDOWS_FACTOR 16/9
#define WINDOWS_WIDTH 405

#define MAX_LOADED_CHUNKS 100
#define CHUNK_LOAD_DISTANCE 3


typedef struct {
    Vector3 position;
    Vector3 velocity;
    float yaw;
    float pitch;
    int id;
} Player;

typedef struct {
    ChunkData data;
    ChunkMesh mesh;
    bool loaded;
} ClientChunk;

// État réseau
NetworkPlayer otherPlayers[MAX_PLAYERS] = {0};
NetworkPlayer playerState = {0};
rnetPeer* client = NULL;
double lastNetworkUpdate = 0;

// État du monde côté client
ClientChunk loadedChunks[MAX_LOADED_CHUNKS];
int loadedChunkCount = 0;

// Fonctions de conversion
static inline Vector3 Vec3ToVector3(Vec3 v) {
    return (Vector3){ v.x, v.y, v.z };
}

static inline Vec3 Vector3ToVec3(Vector3 v) {
    return (Vec3){ v.x, v.y, v.z };
}

// Fonction qui sera utilisée pour générer le terrain
void generateChunk(ChunkData *chunk, int chunkX, int chunkZ) {
    chunk->x = chunkX;
    chunk->z = chunkZ;
    
    // Pour l'instant, générons un terrain plat simple
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int y = 0; y < WORLD_HEIGHT; y++) {
                if (y < 64) {
                    chunk->blocks[x][y][z] = BLOCK_DIRT;
                } else {
                    chunk->blocks[x][y][z] = BLOCK_AIR;
                }
            }
        }
    }
}

// Trouver un chunk chargé
static ClientChunk* findChunk(int x, int z) {
    for (int i = 0; i < loadedChunkCount; i++) {
        if (loadedChunks[i].loaded && loadedChunks[i].data.x == x && loadedChunks[i].data.z == z) {
            return &loadedChunks[i];
        }
    }
    return NULL;
}

// Demander un chunk au serveur
static void requestChunk(int x, int z) {
    Packet packet = {
        .type = PACKET_CHUNK_REQUEST,
        .chunkX = x,
        .chunkZ = z
    };
    rnetSend(client, &packet, sizeof(Packet), RNET_RELIABLE);
}

static void unloadDistantChunks(Vector3 playerPos) {
    int playerChunkX = (int)floor(playerPos.x) / CHUNK_SIZE;
    int playerChunkZ = (int)floor(playerPos.z) / CHUNK_SIZE;
    
    for (int i = 0; i < loadedChunkCount; i++) {
        if (!loadedChunks[i].loaded) continue;
        
        int dx = abs(loadedChunks[i].data.x - playerChunkX);
        int dz = abs(loadedChunks[i].data.z - playerChunkZ);
        
        if (dx > CHUNK_LOAD_DISTANCE + 2 || dz > CHUNK_LOAD_DISTANCE + 2) {
            // Libérer les ressources du chunk
            freeChunkMesh(&loadedChunks[i].mesh);
            loadedChunks[i].loaded = false;
        }
    }
}

// Fonction pour charger/décharger les chunks autour du joueur
static void updateLoadedChunks(Vector3 playerPos) {
    // D'abord décharger les chunks trop éloignés
    unloadDistantChunks(playerPos);
    
    int playerChunkX = (int)floor(playerPos.x) / CHUNK_SIZE;
    int playerChunkZ = (int)floor(playerPos.z) / CHUNK_SIZE;
    
    // Demander les chunks manquants autour du joueur
    for (int x = -CHUNK_LOAD_DISTANCE; x <= CHUNK_LOAD_DISTANCE; x++) {
        for (int z = -CHUNK_LOAD_DISTANCE; z <= CHUNK_LOAD_DISTANCE; z++) {
            int chunkX = playerChunkX + x;
            int chunkZ = playerChunkZ + z;
            
            if (!findChunk(chunkX, chunkZ) && loadedChunkCount < MAX_LOADED_CHUNKS) {
                requestChunk(chunkX, chunkZ);
            }
        }
    }
}

// Ajouter un nouveau chunk aux chunks chargés
static void addChunk(ChunkData* chunk) {
    // Si le chunk existe déjà, le mettre à jour
    ClientChunk* existing = findChunk(chunk->x, chunk->z);
    if (existing) {
        existing->data = *chunk;
        existing->mesh.dirty = true;
        existing->loaded = true;
        return;
    }
    
    // Sinon, ajouter un nouveau chunk
    if (loadedChunkCount < MAX_LOADED_CHUNKS) {
        loadedChunks[loadedChunkCount].data = *chunk;
        loadedChunks[loadedChunkCount].loaded = true;
        memset(&loadedChunks[loadedChunkCount].mesh, 0, sizeof(ChunkMesh));
        loadedChunks[loadedChunkCount].mesh.dirty = true;
        loadedChunkCount++;
    }
}

// Fonction pour convertir des coordonnées monde en coordonnées de bloc
static bool worldToBlockCoords(Vector3 worldPos, Vector3 direction, float maxDist, int* outX, int* outY, int* outZ) {
    float stepSize = 0.1f;
    Vector3 pos = worldPos;
    
    for (float dist = 0; dist < maxDist; dist += stepSize) {
        pos.x = worldPos.x + direction.x * dist;
        pos.y = worldPos.y + direction.y * dist;
        pos.z = worldPos.z + direction.z * dist;
        
        int x = (int)floor(pos.x);
        int y = (int)floor(pos.y);
        int z = (int)floor(pos.z);
        
        // Trouver le chunk qui contient ce block
        int chunkX = x / CHUNK_SIZE;
        int chunkZ = z / CHUNK_SIZE;
        
        // Coordonnées relatives au chunk
        int localX = x % CHUNK_SIZE;
        int localZ = z % CHUNK_SIZE;
        
        if (localX < 0) {
            localX += CHUNK_SIZE;
            chunkX--;
        }
        if (localZ < 0) {
            localZ += CHUNK_SIZE;
            chunkZ--;
        }
        
        ClientChunk* chunk = findChunk(chunkX, chunkZ);
        if (chunk) {
            if (y >= 0 && y < WORLD_HEIGHT && 
                chunk->data.blocks[localX][y][localZ] != BLOCK_AIR) {
                *outX = x;
                *outY = y;
                *outZ = z;
                return true;
            }
        }
    }
    
    return false;
}

void updateNetworkState(Player* player) {
    double currentTime = GetTime();
    
    // Envoyer notre état au serveur toutes les 20ms
    if (currentTime - lastNetworkUpdate >= 1.0/SERVER_TICK_RATE) {
        playerState.position = Vector3ToVec3(player->position);
        playerState.velocity = Vector3ToVec3(player->velocity);
        playerState.yaw = player->yaw;
        playerState.pitch = player->pitch;
        
        Packet packet = {
            .type = PACKET_PLAYER_STATE,
            .player = playerState
        };
        
        rnetSend(client, &packet, sizeof(Packet), RNET_UNRELIABLE);
        lastNetworkUpdate = currentTime;
    }
    
    // Recevoir les mises à jour des autres joueurs
    rnetPacket packet;
    while (rnetReceive(client, &packet)) {
        Packet* gamePacket = (Packet*)packet.data;
        
        switch (gamePacket->type) {
            case PACKET_CONNECT:
                if (playerState.id == 0) {
                    playerState = gamePacket->player;
                    printf("Connected with ID: %d\n", playerState.id);
                }
                break;
            
            case PACKET_PLAYER_STATE: {
                NetworkPlayer* updatedPlayer = &gamePacket->player;
                if (updatedPlayer->id != playerState.id) {
                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        if (!otherPlayers[i].connected || otherPlayers[i].id == updatedPlayer->id) {
                            otherPlayers[i] = *updatedPlayer;
                            otherPlayers[i].connected = true;
                            break;
                        }
                    }
                }
                break;
            }
            
            case PACKET_DISCONNECT: {
                NetworkPlayer* disconnectedPlayer = &gamePacket->player;
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (otherPlayers[i].id == disconnectedPlayer->id) {
                        otherPlayers[i].connected = false;
                        break;
                    }
                }
                break;
            }

            case PACKET_WORLD_STATE:
                // Pour l'instant, on ignore les mises à jour du monde
                break;

            case PACKET_CHUNK_DATA:
                addChunk(&gamePacket->chunk);
                break;
            
            case PACKET_BLOCK_UPDATE: {
                BlockUpdate* update = &gamePacket->blockUpdate;
                int chunkX = update->x / CHUNK_SIZE;
                int chunkZ = update->z / CHUNK_SIZE;
                int localX = update->x % CHUNK_SIZE;
                int localZ = update->z % CHUNK_SIZE;
                
                if (localX < 0) {
                    localX += CHUNK_SIZE;
                    chunkX--;
                }
                if (localZ < 0) {
                    localZ += CHUNK_SIZE;
                    chunkZ--;
                }
                
                ClientChunk* chunk = findChunk(chunkX, chunkZ);
                if (chunk) {
                    chunk->data.blocks[localX][update->y][localZ] = update->type;
                }
                break;
            }
            
            default:
                break;
        }
        
        rnetFreePacket(&packet);
    }
}

int main(void) {
    // Initialisation du réseau
    if (!rnetInit()) {
        printf("Failed to initialize network\n");
        return 1;
    }
    
    client = rnetConnect("127.0.0.1", SERVER_PORT);
    if (!client) {
        printf("Failed to connect to server\n");
        rnetShutdown();
        return 1;
    }
    
    printf("Connecting to server...\n");
    
    // Envoyer une demande de connexion et attendre la réponse
    {
        Packet connectPacket = {
            .type = PACKET_CONNECT
        };
        rnetSend(client, &connectPacket, sizeof(Packet), RNET_RELIABLE);
        
        // Attendre la réponse du serveur (timeout après 5 secondes)
        double startTime = GetTime();
        bool connected = false;
        
        while (!connected && GetTime() - startTime < 5.0) {
            rnetPacket packet;
            if (rnetReceive(client, &packet)) {
                if (packet.data) {
                    Packet* response = (Packet*)packet.data;
                    if (response->type == PACKET_CONNECT) {
                        playerState = response->player;
                        connected = true;
                        printf("Connected to server with ID: %d\n", playerState.id);
                    }
                    rnetFreePacket(&packet);
                }
            }
        }
        
        if (!connected) {
            printf("Failed to receive server response\n");
            rnetClose(client);
            rnetShutdown();
            return 1;
        }
    }
    // Initialisation de la fenêtre
    InitWindow(WINDOWS_WIDTH*WINDOWS_FACTOR, WINDOWS_WIDTH, "Minecraft en C");
    SetTargetFPS(60);
    SetupRenderer();  // Configure OpenGL state
    
    // Initialisation de la caméra
    Camera3D camera = {0};
    camera.position = (Vector3){ 0.0f, 65.0f, 0.0f };
    camera.target = (Vector3){ 0.0f, 65.0f, 1.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Initialisation du joueur
    Player player = {
        .position = (Vector3){ 0.0f, 65.0f, 0.0f },
        .yaw = 0.0f,
        .pitch = 0.0f
    };

    // Création d'un chunk test
    ChunkData testChunk = {0};
    generateChunk(&testChunk, 0, 0);

    // Désactiver le curseur pour le mode FPS
    DisableCursor();

    // Variables pour le material des chunks
    Material chunkMaterial = {0};
    bool materialInitialized = false;

    // Boucle principale
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        
        // Mise à jour du réseau
        updateNetworkState(&player);
        
        // Mise à jour de la caméra FPS
        // Rotation de la caméra
        float mouseX = GetMouseDelta().x * 0.2f;
        float mouseY = GetMouseDelta().y * 0.2f;
        
        player.yaw -= mouseX;
        player.pitch -= mouseY;
        
        // Limiter la rotation verticale
        if (player.pitch > 89.0f) player.pitch = 89.0f;
        if (player.pitch < -89.0f) player.pitch = -89.0f;

        // Calcul des vecteurs de direction
        Vector3 direction = {
            cosf(player.pitch*DEG2RAD) * sinf(player.yaw*DEG2RAD),
            sinf(player.pitch*DEG2RAD),
            cosf(player.pitch*DEG2RAD) * cosf(player.yaw*DEG2RAD)
        };

        // Déplacement du joueur
        float speed = 10.0f * deltaTime;
        if (IsKeyDown(KEY_LEFT_SHIFT)) speed *= 2.0f;
        
        if (IsKeyDown(KEY_W)) {
            player.position.x += direction.x * speed;
            player.position.y += direction.y * speed;
            player.position.z += direction.z * speed;
        }
        if (IsKeyDown(KEY_S)) {
            player.position.x -= direction.x * speed;
            player.position.y -= direction.y * speed;
            player.position.z -= direction.z * speed;
        }

        Vector3 right = (Vector3){ direction.z, 0, -direction.x };
        if (IsKeyDown(KEY_A)) {
            player.position.x += right.x * speed;
            player.position.z += right.z * speed;
        }
        if (IsKeyDown(KEY_F)) {
            player.position.x -= right.x * speed;
            player.position.z -= right.z * speed;
        }

        // Mise à jour des chunks chargés
        updateLoadedChunks(player.position);
        
        // Gestion de la destruction des blocs
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int blockX, blockY, blockZ;
            if (worldToBlockCoords(player.position, direction, 5.0f, &blockX, &blockY, &blockZ)) {
                BlockUpdate update = {
                    .x = blockX,
                    .y = blockY,
                    .z = blockZ,
                    .type = BLOCK_AIR
                };
                
                Packet packet = {
                    .type = PACKET_BLOCK_UPDATE,
                    .blockUpdate = update
                };
                
                rnetSend(client, &packet, sizeof(Packet), RNET_RELIABLE);
            }
        }

        // Mise à jour de la caméra
        camera.position = player.position;
        camera.target = (Vector3){
            player.position.x + direction.x,
            player.position.y + direction.y,
            player.position.z + direction.z
        };

        // Rendu
        BeginDrawing();
            ClearBackground(SKYBLUE);
            
            BeginMode3D(camera);
                // Rendu des chunks
                if (!materialInitialized) {
                    chunkMaterial = LoadMaterialDefault();
                    materialInitialized = true;
                }
                
                // Configure lighting
                Vector3 lightDir = (Vector3){ -0.5f, -1.0f, -0.5f };
                Vector3 lightColor = (Vector3){ 1.0f, 1.0f, 1.0f };
                SetShaderValue(chunkMaterial.shader, GetShaderLocation(chunkMaterial.shader, "lightDir"), &lightDir, SHADER_UNIFORM_VEC3);
                SetShaderValue(chunkMaterial.shader, GetShaderLocation(chunkMaterial.shader, "lightColor"), &lightColor, SHADER_UNIFORM_VEC3);
                
                for (int i = 0; i < loadedChunkCount; i++) {
                    if (!loadedChunks[i].loaded) continue;
                    
                    // Mettre à jour le mesh si nécessaire
                    if (loadedChunks[i].mesh.dirty) {
                        updateChunkMesh(&loadedChunks[i].mesh, &loadedChunks[i].data);
                    }
                    
                    // Calculer la position du chunk dans le monde
                    Vector3 chunkPos = {
                        loadedChunks[i].data.x * CHUNK_SIZE,
                        0,
                        loadedChunks[i].data.z * CHUNK_SIZE
                    };
                    
                    // Dessiner le mesh du chunk
                    if (loadedChunks[i].mesh.initialized) {
                        Matrix transform = MatrixTranslate(chunkPos.x, chunkPos.y, chunkPos.z);
                        DrawMesh(loadedChunks[i].mesh.mesh, chunkMaterial, transform);
                    }
                }
                
                // Rendu des autres joueurs
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (otherPlayers[i].connected && otherPlayers[i].id != playerState.id) {
                        Vector3 bodyPos = Vec3ToVector3(otherPlayers[i].position);
                        Vector3 headPos = bodyPos;
                        headPos.y += 1.0f;
                        RenderCube(bodyPos, 1.0f, 1.0f, 1.0f, BROWN);
                        RenderCube(headPos, 1.0f, 1.0f, 1.0f, RED);
                        
                        // Direction du regard
                        Vector3 direction = {
                            cosf(otherPlayers[i].pitch*DEG2RAD) * sinf(otherPlayers[i].yaw*DEG2RAD),
                            sinf(otherPlayers[i].pitch*DEG2RAD),
                            cosf(otherPlayers[i].pitch*DEG2RAD) * cosf(otherPlayers[i].yaw*DEG2RAD)
                        };
                        
                        RenderLine3D(
                            headPos,
                            (Vector3){
                                headPos.x + direction.x * 2.0f,
                                headPos.y + direction.y * 2.0f,
                                headPos.z + direction.z * 2.0f
                            },
                            BLUE
                        );
                    }
                }
            EndMode3D();

            // UI
            DrawFPS(10, 10);
            RenderText("ZQSD pour se déplacer, Souris pour regarder", 10, 30, 20, WHITE);
            RenderText(TextFormat("Position: %.2f, %.2f, %.2f", 
                              player.position.x, 
                              player.position.y, 
                              player.position.z), 10, 50, 20, WHITE);
            
            // Afficher le nombre de joueurs connectés
            int playerCount = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (otherPlayers[i].connected) playerCount++;
            }
            RenderText(TextFormat("Players online: %d", playerCount + 1), 10, 70, 20, WHITE);
        EndDrawing();
    }

    // Nettoyage
    if (materialInitialized) {
        UnloadMaterial(chunkMaterial);
    }
    
    for (int i = 0; i < loadedChunkCount; i++) {
        if (loadedChunks[i].loaded) {
            freeChunkMesh(&loadedChunks[i].mesh);
        }
    }
    rnetClose(client);
    rnetShutdown();
    CloseWindow();
    return 0;
}
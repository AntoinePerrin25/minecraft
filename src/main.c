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
#include <string.h>  // Add string.h for memset
#include <math.h>
#include "chunk_mesh.h"
#include "chunk_thread.h"
#include "rnet.h"

#define WINDOWS_FACTOR 16/9
#define WINDOWS_WIDTH 405

typedef struct {
    Vector3 position;
    Vector3 velocity;
    float yaw;
    float pitch;
    int id;
} Player;

// État réseau
NetworkPlayer otherPlayers[MAX_PLAYERS] = {0};
NetworkPlayer playerState = {0};
rnetPeer* client = NULL;
double lastNetworkUpdate = 0;

// État du monde côté client
ChunkThreadManager chunkManager;

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
        if (packet.data) {
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
                    handleChunkUpdate(&chunkManager, &gamePacket->chunk);
                    break;
                
                default:
                    break;
            }
            
            rnetFreePacket(&packet);
        }
    }
}

int main(void) {
    // Initialisation de la fenêtre d'abord
    InitWindow(WINDOWS_WIDTH*WINDOWS_FACTOR, WINDOWS_WIDTH, "Minecraft en C");
    SetTargetFPS(60);
    SetupRenderer();  // Configure OpenGL state
    
    // Initialisation de la caméra et du joueur
    Player player = {
        .position = (Vector3){ 0.0f, 70.0f, 0.0f }, // Start a bit higher to see terrain
        .yaw = 0.0f,
        .pitch = 0.0f
    };

    Camera3D camera = {0};
    camera.position = player.position;
    camera.target = (Vector3){ 0.0f, 70.0f, 1.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Désactiver le curseur pour le mode FPS
    DisableCursor();

    // Initialisation du réseau
    if (!rnetInit()) {
        printf("Failed to initialize network\n");
        CloseWindow();
        return 1;
    }
    
    client = rnetConnect("127.0.0.1", SERVER_PORT);
    if (!client) {
        printf("Failed to connect to server\n");
        rnetShutdown();
        CloseWindow();
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
                        player.position = Vec3ToVector3(playerState.position);
                        camera.position = player.position;
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
            CloseWindow();
            return 1;
        }
    }

    // Initialize chunk threading system
    initChunkThreadManager(&chunkManager, client);

    // Create default material for chunks
    Material chunkMaterial = LoadMaterialDefault();
    bool materialInitialized = true;

    // Configure lighting for better visibility
    Vector3 lightDir = (Vector3){ -0.5f, -1.0f, -0.5f };
    Vector3 lightColor = (Vector3){ 1.0f, 1.0f, 1.0f };
    float ambientStrength = 0.3f;
    SetShaderValue(chunkMaterial.shader, GetShaderLocation(chunkMaterial.shader, "lightDir"), &lightDir, SHADER_UNIFORM_VEC3);
    SetShaderValue(chunkMaterial.shader, GetShaderLocation(chunkMaterial.shader, "lightColor"), &lightColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(chunkMaterial.shader, GetShaderLocation(chunkMaterial.shader, "ambient"), &ambientStrength, SHADER_UNIFORM_FLOAT);

    // Boucle principale
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        static double lastDebugPrint = 0;
        double currentTime = GetTime();
        
        // Debug prints every second
        if (currentTime - lastDebugPrint >= 1.0) {
            int loadedChunks = 0;
            int initializedMeshes = 0;
            
            for (int i = 0; i < chunkManager.chunkCount; i++) {
                ThreadedChunk* chunk = &chunkManager.chunks[i];
                pthread_mutex_lock(&chunk->mutex);
                if (chunk->loaded) loadedChunks++;
                if (chunk->loaded && chunk->mesh.initialized) initializedMeshes++;
                pthread_mutex_unlock(&chunk->mutex);
            }
            
            printf("\n=== Client Debug Info ===\n");
            printf("Total chunks in manager: %d\n", chunkManager.chunkCount);
            printf("Loaded chunks: %d\n", loadedChunks);
            printf("Initialized meshes: %d\n", initializedMeshes);
            printf("Player chunk position: %d, %d\n", 
                   (int)floor(player.position.x) / CHUNK_SIZE,
                   (int)floor(player.position.z) / CHUNK_SIZE);
            printf("=====================\n\n");
            
            lastDebugPrint = currentTime;
        }
        
        // Update network state first
        updateNetworkState(&player);
        
        // Camera movement and controls
        float mouseX = GetMouseDelta().x * 0.2f;
        float mouseY = GetMouseDelta().y * 0.2f;
        
        player.yaw -= mouseX;
        player.pitch -= mouseY;
        
        if (player.pitch > 89.0f) player.pitch = 89.0f;
        if (player.pitch < -89.0f) player.pitch = -89.0f;

        Vector3 direction = {
            cosf(player.pitch*DEG2RAD) * sinf(player.yaw*DEG2RAD),
            sinf(player.pitch*DEG2RAD),
            cosf(player.pitch*DEG2RAD) * cosf(player.yaw*DEG2RAD)
        };

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
        if (IsKeyDown(KEY_D)) {  // Fixed key from F to D
            player.position.x -= right.x * speed;
            player.position.z -= right.z * speed;
        }

        // Update camera
        camera.position = player.position;
        camera.target = Vector3Add(camera.position, direction);

        // Request chunks in view distance
        int playerChunkX = (int)floor(player.position.x) / CHUNK_SIZE;
        int playerChunkZ = (int)floor(player.position.z) / CHUNK_SIZE;
        
        for (int x = -CHUNK_LOAD_DISTANCE; x <= CHUNK_LOAD_DISTANCE; x++) {
            for (int z = -CHUNK_LOAD_DISTANCE; z <= CHUNK_LOAD_DISTANCE; z++) {
                requestChunk(&chunkManager, playerChunkX + x, playerChunkZ + z);
            }
        }

        // Rendering
        BeginDrawing();
            ClearBackground(SKYBLUE);
            
            BeginMode3D(camera);
                // Render chunks
                for (int i = 0; i < chunkManager.chunkCount; i++) {
                    ThreadedChunk* chunk = &chunkManager.chunks[i];
                    pthread_mutex_lock(&chunk->mutex);
                    
                    if (chunk->loaded && chunk->mesh.initialized) {
                        Vector3 chunkPos = {
                            chunk->data.x * CHUNK_SIZE,
                            0,
                            chunk->data.z * CHUNK_SIZE
                        };
                        Matrix transform = MatrixTranslate(chunkPos.x, chunkPos.y, chunkPos.z);
                        DrawMesh(chunk->mesh.mesh, chunkMaterial, transform);
                    }
                    
                    pthread_mutex_unlock(&chunk->mutex);
                }
                
                // Render other players
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (otherPlayers[i].connected && otherPlayers[i].id != playerState.id) {
                        Vector3 bodyPos = Vec3ToVector3(otherPlayers[i].position);
                        Vector3 headPos = bodyPos;
                        headPos.y += 1.0f;
                        DrawCube(bodyPos, 1.0f, 1.0f, 1.0f, BROWN);
                        DrawCube(headPos, 1.0f, 1.0f, 1.0f, RED);
                    }
                }
            EndMode3D();

            // UI
            DrawFPS(10, 10);
            DrawText("WASD to move, Mouse to look", 10, 30, 20, WHITE);
            DrawText(TextFormat("Position: %.2f, %.2f, %.2f", 
                              player.position.x, 
                              player.position.y, 
                              player.position.z), 10, 50, 20, WHITE);
            
            int playerCount = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (otherPlayers[i].connected) playerCount++;
            }
            DrawText(TextFormat("Players online: %d", playerCount + 1), 10, 70, 20, WHITE);
            
            // Add chunk debug info
            int loadedChunks = 0;
            int initializedMeshes = 0;
            for (int i = 0; i < chunkManager.chunkCount; i++) {
                ThreadedChunk* chunk = &chunkManager.chunks[i];
                pthread_mutex_lock(&chunk->mutex);
                if (chunk->loaded) loadedChunks++;
                if (chunk->loaded && chunk->mesh.initialized) initializedMeshes++;
                pthread_mutex_unlock(&chunk->mutex);
            }
            DrawText(TextFormat("Total Chunks: %d", chunkManager.chunkCount), 10, 90, 20, WHITE);
            DrawText(TextFormat("Loaded Chunks: %d", loadedChunks), 10, 110, 20, WHITE);
            DrawText(TextFormat("Rendered Meshes: %d", initializedMeshes), 10, 130, 20, WHITE);
            
        EndDrawing();
    }

    // Cleanup
    destroyChunkThreadManager(&chunkManager);
    if (materialInitialized) {
        UnloadMaterial(chunkMaterial);
    }
    
    rnetClose(client);
    rnetShutdown();
    CloseWindow();
    return 0;
}
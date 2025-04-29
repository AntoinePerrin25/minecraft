#ifdef _WIN32
#define WINDOWS_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#endif

#define NOB_IMPLEMENTATION
#include "../nob.h"

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

#include "chunk_manager.h"
#include "world_manager.h"
//#include "chunk_mesh.h"
#include "world.h"

// Définir NETWORK_IMPL après tous les autres includes
#define NETWORK_IMPL
#include "rnet.h"

#define WINDOWS_FACTOR 16/9
#define WINDOWS_WIDTH 405

#define MAX_LOADED_CHUNKS 100
#define CHUNK_LOAD_DISTANCE 3

// État réseau
NetworkPlayer otherPlayers[MAX_PLAYERS] = {0};
NetworkPlayer playerState = {0};
rnetPeer* client = NULL;
double lastNetworkUpdate = 0;
WorldManager* worldManager = NULL;

// Fonctions de conversion
static inline Vector3 Vec3ToVector3(Vec3 v) {
    return (Vector3){ v.x, v.y, v.z };
}

static inline Vec3 Vector3ToVec3(Vector3 v) {
    return (Vec3){ v.x, v.y, v.z };
}

static void requestChunk(int x, int z) {
    Packet packet = {
        .type = PACKET_CHUNK_REQUEST,
        .chunkX = x,
        .chunkZ = z
    };
    rnetSend(client, &packet, sizeof(Packet), RNET_RELIABLE);
}

// Trouver un chunk chargé ou un slot libre
static ClientChunk* findOrCreateClientChunk(ChunkManager* manager, int x, int z) {
    // D'abord chercher un chunk existant
    ClientChunk* chunk = GetChunk(manager, x, z);
    if (chunk) return chunk;
    
    // Créer un nouveau chunk
    ClientChunk* newChunk = CreateClientChunk(x, z);
    if (!newChunk) {
        nob_log(NOB_ERROR, "Failed to create new chunk at (%d, %d)", x, z);
        return NULL;
    }
    
    // Get the chunk data from world manager
    FullChunk* chunkData = worldManager_getChunk(worldManager, x, z);
    if (!chunkData) {
        nob_log(NOB_ERROR, "Failed to get chunk data for (%d, %d)", x, z);
        FreeClientChunk(newChunk);
        return NULL;
    }
    
    // Copy the chunk data
    memcpy(&newChunk->chunk, chunkData, sizeof(FullChunk));
    newChunk->loaded = true;
    
    // Add the chunk to the manager
    AddChunk(manager, newChunk);
    
    return newChunk;
}

// Fonction pour charger/décharger les chunks autour du joueur
static void updateLoadedChunks(ChunkManager* manager, Vector3 playerPos) {
    // Unload distant chunks
    unloadDistantChunks(manager, &playerPos);
    
    // Calculate player chunk position
    Vector3Int playerChunk = worldToChunkCoords(&playerPos);
    
    // Load chunks around player
    for (int dx = -CHUNK_LOAD_DISTANCE; dx <= CHUNK_LOAD_DISTANCE; dx++) {
        for (int dz = -CHUNK_LOAD_DISTANCE; dz <= CHUNK_LOAD_DISTANCE; dz++) {
            int chunkX = playerChunk.x + dx;
            int chunkZ = playerChunk.z + dz;
            
            // Check if chunk is already loaded
            ClientChunk* chunk = GetChunk(manager, chunkX, chunkZ);
            if (!chunk) {
                // Try to load from world manager
                findOrCreateClientChunk(manager, chunkX, chunkZ);
            }
        }
    }
}

// Modifie un bloc dans le monde
static bool setBlock(int x, int y, int z, BlockType type) {
    // Create a block data structure
    BlockData block = {0};
    block.Type = type;
    block.solid = (type != BLOCK_AIR && type != BLOCK_WATER);
    block.visible = (type != BLOCK_AIR);
    
    // Set the block in the world
    if (worldManager_setBlock(worldManager, x, y, z, block)) {
        // If successful, find the chunk and mark it for mesh update
        int chunkX = floor((float)x / CHUNK_SIZE);
        int chunkZ = floor((float)z / CHUNK_SIZE);
        
        ClientChunk* chunk = GetChunk(ChunkManager, chunkX, chunkZ);
        if (chunk) {
            chunk->mesh.dirty = true;  // Mark for mesh update
        }
        
        return true;
    }
    
    return false;
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
        
        // Check if this position has a solid block
        if (y >= 0 && y < WORLD_HEIGHT) {
            int chunkX = floor((float)x / CHUNK_SIZE);
            int chunkZ = floor((float)z / CHUNK_SIZE);
            
            // Local coordinates in the chunk
            int localX = x - chunkX * CHUNK_SIZE;
            int localZ = z - chunkZ * CHUNK_SIZE;
            if (localX < 0) localX += CHUNK_SIZE;
            if (localZ < 0) localZ += CHUNK_SIZE;
            
            // Get the block type
            FullChunk* chunk = worldManager_getChunk(worldManager, chunkX, chunkZ);
            if (chunk) {
                // Get block type directly from chunk
                BlockType blockType = chunk->blocks[y][localX][localZ].Type;
                
                // Check if the block is solid
                if (blockType != BLOCK_AIR) {
                    *outX = x;
                    *outY = y;
                    *outZ = z;
                    return true;
                }
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

            case PACKET_BLOCK_UPDATE: {
                BlockUpdate* update = &gamePacket->blockUpdate;
                
                // Create a block data structure
                BlockData block = {0};
                block.Type = update->block.Type;
                block.solid = update->block.solid;
                block.visible = update->block.visible;
                
                // Update the block in the world
                if (worldManager_setBlock(worldManager, update->blockpos.x, update->blockpos.y, update->blockpos.z, block)) {
                    int chunkX = floor((float)update->blockpos.x / CHUNK_SIZE);
                    int chunkZ = floor((float)update->blockpos.z / CHUNK_SIZE);
                    
                    // Mark the chunk for mesh update
                    ClientChunk* chunk = GetChunk(ChunkManager, chunkX, chunkZ);
                    if (chunk) {
                        chunk->mesh.dirty = true;
                    }
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
    // Create world manager with a random seed
    int worldSeed = (int)GetTime();
    worldManager = worldManager_create(worldSeed);
    if (!worldManager) {
        printf("Failed to create world manager\n");
        return 1;
    }
    
    // Initialisation du réseau
    if (!rnetInit()) {
        printf("Failed to initialize network\n");
        worldManager_destroy(worldManager);
        return 1;
    }
    
    client = rnetConnect("127.0.0.1", SERVER_PORT);
    if (!client) {
        printf("Failed to connect to server\n");
        rnetShutdown();
        worldManager_destroy(worldManager);
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
            worldManager_destroy(worldManager);
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

    // Initialiser le gestionnaire de chunks
    ChunkManager* chunkManager = InitChunkManager(MAX_LOADED_CHUNKS);
    if (!chunkManager) {
        printf("Failed to allocate chunk manager\n");
        rnetClose(client);
        rnetShutdown();
        worldManager_destroy(worldManager);
        return 1;
    }

    // Désactiver le curseur pour le mode FPS
    DisableCursor();

    // Variables pour le material des chunks
    Material chunkMaterial = LoadMaterialDefault();

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
        if (IsKeyDown(KEY_D)) {
            player.position.x -= right.x * speed;
            player.position.z -= right.z * speed;
        }

        // Mise à jour des chunks chargés
        updateLoadedChunks(chunkManager, player.position);
        
        // Gestion de la destruction/placement des blocs
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            // Destroy block
            int blockX, blockY, blockZ;
            if (worldToBlockCoords(player.position, direction, 5.0f, &blockX, &blockY, &blockZ)) {
                setBlock(blockX, blockY, blockZ, BLOCK_AIR);
                
                // Send to server
                BlockData block = {0};
                block.Type = BLOCK_AIR;
                block.visible = 0;
                block.solid = 0;
                
                BlockUpdate update = {0};
                update.blockpos.x = blockX;
                update.blockpos.y = blockY;
                update.blockpos.z = blockZ;
                update.block = block;
                
                Packet packet = {
                    .type = PACKET_BLOCK_UPDATE,
                    .blockUpdate = update
                };
                
                rnetSend(client, &packet, sizeof(Packet), RNET_RELIABLE);
            }
        }
        else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            // Place block
            int blockX, blockY, blockZ;
            if (worldToBlockCoords(player.position, direction, 5.0f, &blockX, &blockY, &blockZ)) {
                // Move to the adjacent block face
                Vector3 hitPos = {
                    player.position.x + direction.x * 5.0f,
                    player.position.y + direction.y * 5.0f,
                    player.position.z + direction.z * 5.0f
                };
                
                // Determine which face was hit
                float dx = hitPos.x - (blockX + 0.5f);
                float dy = hitPos.y - (blockY + 0.5f);
                float dz = hitPos.z - (blockZ + 0.5f);
                
                // Find the largest component to determine face
                if (fabs(dx) > fabs(dy) && fabs(dx) > fabs(dz)) {
                    blockX += (dx > 0) ? 1 : -1;
                } else if (fabs(dy) > fabs(dx) && fabs(dy) > fabs(dz)) {
                    blockY += (dy > 0) ? 1 : -1;
                } else {
                    blockZ += (dz > 0) ? 1 : -1;
                }
                
                // Create a dirt block
                BlockData block = {0};
                block.Type = BLOCK_DIRT;
                block.visible = 1;
                block.solid = 1;
                
                if (worldManager_setBlock(worldManager, blockX, blockY, blockZ, block)) {
                    // If successful, update the chunk mesh
                    int chunkX = floor((float)blockX / CHUNK_SIZE);
                    int chunkZ = floor((float)blockZ / CHUNK_SIZE);
                    
                    ClientChunk* chunk = GetChunk(chunkManager, chunkX, chunkZ);
                    if (chunk) {
                        chunk->mesh.dirty = true;
                    }
                    
                    // Send to server
                    BlockUpdate update = {0};
                    update.blockpos.x = blockX;
                    update.blockpos.y = blockY;
                    update.blockpos.z = blockZ;
                    update.block = block;
                    
                    Packet packet = {
                        .type = PACKET_BLOCK_UPDATE,
                        .blockUpdate = update
                    };
                    
                    rnetSend(client, &packet, sizeof(Packet), RNET_RELIABLE);
                }
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
                for (int i = 0; i < chunkManager->capacity; i++) {
                    ClientChunk* chunk = chunkManager->chunks[i];
                    if (!chunk || !chunk->loaded) continue;
                    
                    if (chunk->mesh.dirty) {
                        updateChunkMesh(&chunk->mesh, &chunk->data);
                    }
                    
                    // Calculer la position du chunk dans le monde
                    Vector3 chunkPos = {
                        chunk->x * CHUNK_SIZE,
                        0,
                        chunk->z * CHUNK_SIZE
                    };
                    
                    // Dessiner le mesh du chunk
                    if (chunk->mesh.initialized) {
                        Matrix transform = MatrixTranslate(chunkPos.x, chunkPos.y, chunkPos.z);
                        DrawMesh(chunk->mesh.mesh, chunkMaterial, transform);
                    }
                }
                
                // Rendu des autres joueurs
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (otherPlayers[i].connected && otherPlayers[i].id != playerState.id) {
                        Vector3 bodyPos = Vec3ToVector3(otherPlayers[i].position);
                        Vector3 headPos = bodyPos;
                        headPos.y += 1.0f;
                        DrawCube(bodyPos, 1.0f, 1.0f, 1.0f, BROWN);
                        DrawCube(headPos, 1.0f, 1.0f, 1.0f, RED);
                        
                        // Direction du regard
                        Vector3 direction = {
                            cosf(otherPlayers[i].pitch*DEG2RAD) * sinf(otherPlayers[i].yaw*DEG2RAD),
                            sinf(otherPlayers[i].pitch*DEG2RAD),
                            cosf(otherPlayers[i].pitch*DEG2RAD) * cosf(otherPlayers[i].yaw*DEG2RAD)
                        };
                        
                        DrawLine3D(
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
            DrawText("WASD pour se déplacer, Souris pour regarder", 10, 30, 20, WHITE);
            DrawText(TextFormat("Position: %.2f, %.2f, %.2f", 
                              player.position.x, 
                              player.position.y, 
                              player.position.z), 10, 50, 20, WHITE);
            
            // Afficher le nombre de joueurs connectés et de chunks
            int playerCount = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (otherPlayers[i].connected) playerCount++;
            }
            DrawText(TextFormat("Players online: %d", playerCount + 1), 10, 70, 20, WHITE);
            DrawText(TextFormat("Loaded chunks: %d/%d", chunkManager->count, MAX_LOADED_CHUNKS), 10, 90, 20, WHITE);
        EndDrawing();
    }

    // Nettoyage
    if (chunkMaterial.maps != NULL) {
        UnloadMaterial(chunkMaterial);
    }
    
    // Libérer tous les chunks
    FreeChunkManager(chunkManager);
    
    // Save world
    worldManager_saveAll(worldManager);
    worldManager_destroy(worldManager);

    rnetClose(client);
    rnetShutdown();
    CloseWindow();
    return 0;
}
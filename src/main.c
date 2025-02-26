#ifdef _WIN32
#define WINDOWS_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#endif

// Inclure raylib avant les autres headers pour éviter les conflits
#include "raylib.h"
#include "renderer.h"

// Autres includes après raylib
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Définir NETWORK_IMPL après tous les autres includes
#define NETWORK_IMPL
#include "rnet.h"

#define CHUNK_SIZE 16
#define WORLD_HEIGHT 256
#define RENDER_DISTANCE 8

#define WINDOWS_FACTOR 16/9
#define WINDOWS_WIDTH 405

typedef unsigned char BlockType;

typedef struct {
    BlockType blocks[CHUNK_SIZE][WORLD_HEIGHT][CHUNK_SIZE];
    bool modified;
    int x, z;  // Chunk coordinates
} Chunk;

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

// Fonctions de conversion
static inline Vector3 Vec3ToVector3(Vec3 v) {
    return (Vector3){ v.x, v.y, v.z };
}

static inline Vec3 Vector3ToVec3(Vector3 v) {
    return (Vec3){ v.x, v.y, v.z };
}

// Fonction qui sera utilisée pour générer le terrain
void generateChunk(Chunk *chunk, int chunkX, int chunkZ) {
    chunk->x = chunkX;
    chunk->z = chunkZ;
    chunk->modified = true;
    
    // Pour l'instant, générons un terrain plat simple
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int y = 0; y < WORLD_HEIGHT; y++) {
                if (y < 64) {
                    chunk->blocks[x][y][z] = 1; // Terre
                } else {
                    chunk->blocks[x][y][z] = 0; // Air
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
    Chunk testChunk = {0};
    generateChunk(&testChunk, 0, 0);

    // Désactiver le curseur pour le mode FPS
    DisableCursor();

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
                // Rendu du chunk test
                for (int x = 0; x < CHUNK_SIZE; x++) {
                    for (int z = 0; z < CHUNK_SIZE; z++) {
                        for (int y = 0; y < WORLD_HEIGHT; y++) {
                            if (testChunk.blocks[x][y][z] != 0) {
                                DrawCube((Vector3){x, y, z}, 1.0f, 1.0f, 1.0f, BROWN);
                            }
                        }
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
    rnetClose(client);
    rnetShutdown();
    CloseWindow();
    return 0;
}
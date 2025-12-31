// Inclure raylib avant les autres headers pour éviter les conflits

#include "data.h"
#include "atlas.h"
#include "mesh.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>




int main(void) {
    // Initialisation de la fenêtre
    InitWindow(WINDOWS_WIDTH, WINDOWS_HEIGHT, "Minecraft en C");
    SetTargetFPS(120);
    DisableCursor(); // Cacher le curseur pour la caméra FPS

    // Charger l'atlas de textures
    Texture2D blockAtlas = LoadAtlasTexture("atlas.png");
    if (blockAtlas.id == 0) {
        printf("ERREUR: Impossible de charger atlas.png\n");
        return 1;
    }
    
    // Initialisation de la caméra
    Camera3D camera = {0};
    camera.position = (Vector3){ 0.0f, 65.0f, 0.0f };
    camera.target = (Vector3){ 0.0f, 65.0f, 1.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    // Initialisation du joueur
    Player player = {
        .position = (Vector3){ 0.0f, 66.0f, 0.0f },
        .yaw = 0.0f,
        .pitch = 0.0f
    };

    // Initialisation d'un pool de chunks
    int maxChunks = (2*CHUNK_LOAD_DISTANCE+1)*(2*CHUNK_LOAD_DISTANCE+1);
    Chunk* chunks = malloc(maxChunks * sizeof(Chunk));
    
    // Initialiser tous les chunks comme inactifs
    for (int i = 0; i < maxChunks; i++) {
        chunks[i].active = 0;
        chunks[i].loaded = 0;
        chunks[i].render.hasMesh = 0;
        chunks[i].render.meshReady = 0;
    }

    // Définir le nombre total de chunks pour getBlockAt()
    setGlobalChunkCount(maxChunks);
    
    // Initialiser les générateurs Perlin noise
    initGenerators(WORLD_SEED);
    
    // Initialiser le système de mesh (workers + queues)
    InitMeshSystem(chunks, maxChunks, blockAtlas);
    
    // Variables pour le chargement dynamique
    Vector2Int lastPlayerChunk = {-9999, -9999}; // Valeur invalide pour forcer le premier chargement

    // Boucle principale
    while (!WindowShouldClose())
    {
        float deltaTime = GetFrameTime();
        // Mise à jour de la caméra FPS
        // Rotation de la caméra
        float mouseX = GetMouseDelta().x * 0.2f;
        float mouseY = GetMouseDelta().y * 0.2f;
        
        player.yaw -= mouseX;
        player.pitch -= mouseY;
        
        // Limiter la rotation verticale
        if (player.pitch > 89.9999f) player.pitch = 89.9999f;
        if (player.pitch < -89.9999f) player.pitch = -89.9999f;

        // Calcul des vecteurs de direction
        Vector3 direction = {
            cosf(player.pitch*DEG2RAD) * sinf(player.yaw*DEG2RAD),
            sinf(player.pitch*DEG2RAD),
            cosf(player.pitch*DEG2RAD) * cosf(player.yaw*DEG2RAD)
        };

        // Déplacement du joueur
        float speed = 10.0f * deltaTime;
        if (IsKeyDown(KEY_LEFT_SHIFT)) speed *= 2.5f;
        
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
        if (IsKeyDown(KEY_SPACE)) {
            player.position.y += speed;
        }

        // Mise à jour de la caméra
        camera.position = player.position;
        camera.target = (Vector3){
            player.position.x + direction.x,
            player.position.y + direction.y,
            player.position.z + direction.z
        };

        // Gestion du chargement/déchargement de chunks
        Vector2Int currentPlayerChunk = {
            (int)floorf(player.position.x / CHUNK_SIZE),
            (int)floorf(player.position.z / CHUNK_SIZE)
        };
        
        // Vérifier si le joueur a changé de chunk
        if (currentPlayerChunk.x != lastPlayerChunk.x || currentPlayerChunk.z != lastPlayerChunk.z) {
            // Marquer les chunks qui doivent être déchargés
            for (int i = 0; i < maxChunks; i++) {
                if (!chunks[i].active) continue;
                
                int dx = chunks[i].x - currentPlayerChunk.x;
                int dz = chunks[i].z - currentPlayerChunk.z;
                
                // Si le chunk est trop loin, le décharger
                if (abs(dx) > CHUNK_LOAD_DISTANCE || abs(dz) > CHUNK_LOAD_DISTANCE) {
                    UnloadChunkMesh(i);
                    chunks[i].active = 0;
                    chunks[i].loaded = 0;
                }
            }
            
            // Charger les nouveaux chunks autour du joueur
            for (int x = -CHUNK_LOAD_DISTANCE; x <= CHUNK_LOAD_DISTANCE; x++) {
                for (int z = -CHUNK_LOAD_DISTANCE; z <= CHUNK_LOAD_DISTANCE; z++) {
                    int chunkX = currentPlayerChunk.x + x;
                    int chunkZ = currentPlayerChunk.z + z;
                    
                    // Vérifier si ce chunk existe déjà
                    int found = 0;
                    for (int i = 0; i < maxChunks; i++) {
                        if (chunks[i].active && chunks[i].x == chunkX && chunks[i].z == chunkZ) {
                            found = 1;
                            break;
                        }
                    }
                    
                    // Si le chunk n'existe pas, le créer
                    if (!found) {
                        // Trouver un slot libre
                        for (int i = 0; i < maxChunks; i++) {
                            if (!chunks[i].active) {
                                generateChunk(&chunks[i], chunkX, chunkZ);
                                chunks[i].active = 1;
                                chunks[i].loaded = 1;
                                ScheduleChunkRemesh(i, 0);
                                break;
                            }
                        }
                    }
                }
            }
            
            lastPlayerChunk = currentPlayerChunk;
        }

        // Poll mesh uploads (main thread uploads ready meshes to GPU)
        PollMeshUploads();

        // Rendu
        BeginDrawing();
            ClearBackground(SKYBLUE);
            
            BeginMode3D(camera);
            // Draw directions arrows (x :red, y:green, z:blue)
            DrawGrid(100, 1.0f);
            DrawLine3D((Vector3){0,0,0}, (Vector3){10,0,0}, RED);
            DrawLine3D((Vector3){0,0,0}, (Vector3){0,10,0}, GREEN);
            DrawLine3D((Vector3){0,0,0}, (Vector3){0,0,10}, BLUE);

            // Draw chunk meshes
            DrawChunks(chunks, camera, player.position);

            EndMode3D();

            // UI Player
            // Draw crosshair
            DrawText("+", GetScreenWidth()/2 - 5, GetScreenHeight()/2 - 5, 20, WHITE);


            // UI Debug
            DrawFPS(10, 10);
            DrawText(TextFormat("Position: %.2f, %.2f, %.2f", 
                              player.position.x, 
                              player.position.y, 
                              player.position.z), 10, 50, 20, WHITE);
            
        EndDrawing();
    }

    // Maintenant on peut libérer l'atlas car plus aucun material ne le référence
    UnloadTexture(blockAtlas);
    
    // Libérer le tableau de chunks
    // Shutdown mesh system and free resources
    ShutdownMeshSystem();
    free(chunks);

    CloseWindow();
    return 0;
}
// Inclure raylib avant les autres headers pour éviter les conflits

#include "data.h"
#include "atlas.h"

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

    // Initialisation des chunks
    Chunk* chunks = malloc((2*RENDER_DISTANCE+1)*(2*RENDER_DISTANCE+1) * sizeof(Chunk));
    for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; x++) {
        for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; z++) {
            int index = (x + RENDER_DISTANCE) * (2*RENDER_DISTANCE + 1) + (z + RENDER_DISTANCE);
            // Initialiser les pointeurs du Model à NULL pour éviter les crashs
            generateChunk(&chunks[index], x, z);
        }
    }   

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

        // Rendu
        BeginDrawing();
            ClearBackground(SKYBLUE);
            
            BeginMode3D(camera);
            // Draw directions arrows (x :red, y:green, z:blue)
            DrawGrid(100, 1.0f);
            DrawLine3D((Vector3){0,0,0}, (Vector3){10,0,0}, RED);
            DrawLine3D((Vector3){0,0,0}, (Vector3){0,10,0}, GREEN);
            DrawLine3D((Vector3){0,0,0}, (Vector3){0,0,10}, BLUE);
            
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
    free(chunks);

    CloseWindow();
    return 0;
}
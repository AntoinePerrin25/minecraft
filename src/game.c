#include "game.h"

#include "atlas.h"
#include "chunk_pool.h"
#include "config.h"
#include "mesh.h"
#include "player.h"

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>

static void DrawDebugOverlay(const Player *player)
{
    DrawText("+", GetScreenWidth() / 2 - 5, GetScreenHeight() / 2 - 5, 20, WHITE);
    DrawFPS(10, 10);
    DrawText(TextFormat("Position: %.2f, %.2f, %.2f",
                        player->position.x,
                        player->position.y,
                        player->position.z),
             10, 50, 20, WHITE);
}

static void DrawWorld(const ChunkPool *pool, Camera3D camera, Vector3 playerPos)
{
    DrawGrid(100, 1.0f);
    DrawLine3D((Vector3){ 0, 0, 0 }, (Vector3){ 10, 0, 0 }, RED);
    DrawLine3D((Vector3){ 0, 0, 0 }, (Vector3){ 0, 10, 0 }, GREEN);
    DrawLine3D((Vector3){ 0, 0, 0 }, (Vector3){ 0, 0, 10 }, BLUE);
    DrawChunks(pool->chunks, camera, playerPos);
}

int GameRun(void)
{
    InitWindow(WINDOWS_WIDTH, WINDOWS_HEIGHT, "Minecraft en C");
    SetTargetFPS(60);
    DisableCursor();

    Texture2D blockAtlas = LoadAtlasTexture(ATLAS_PATH);
    if (blockAtlas.id == 0) {
        printf("ERREUR: Impossible de charger %s\n", ATLAS_PATH);
        CloseWindow();
        return 1;
    }

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 65.0f, 0.0f };
    camera.target = (Vector3){ 0.0f, 65.0f, 1.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Player player;
    PlayerInit(&player);

    ChunkPool pool;
    ChunkPoolInit(&pool);
    InitMeshSystem(pool.chunks, pool.maxChunks, blockAtlas);

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        PlayerUpdate(&player, deltaTime);
        PlayerSyncCamera(&player, &camera, NULL);
        ChunkPoolUpdate(&pool, PlayerChunkCoord(&player));
        PollMeshUploads();

        BeginDrawing();
        ClearBackground(SKYBLUE);

        BeginMode3D(camera);
        DrawWorld(&pool, camera, player.position);
        EndMode3D();

        DrawDebugOverlay(&player);
        EndDrawing();
    }

    UnloadTexture(blockAtlas);
    ShutdownMeshSystem();
    ChunkPoolFree(&pool);
    CloseWindow();
    return 0;
}

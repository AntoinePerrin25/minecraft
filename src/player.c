#include "player.h"

#include "camera_fps.h"
#include "config.h"
#include "raymath.h"

#include <math.h>

void PlayerInit(Player *player)
{
    player->position = (Vector3){ 0.0f, 66.0f, 0.0f };
    player->velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
    FpsLookInit(&player->look);
    player->id = 0;
}

void PlayerUpdate(Player *player, float deltaTime)
{
    float mouseX = GetMouseDelta().x * 0.2f;
    float mouseY = GetMouseDelta().y * 0.2f;

    FpsLookApplyMouse(&player->look, mouseX, mouseY);
    FpsBasis basis = FpsLookGetBasis(&player->look);

    float speed = 10.0f * deltaTime;
    if (IsKeyDown(KEY_LEFT_SHIFT)) speed *= 2.5f;

    if (IsKeyDown(KEY_W)) {
        player->position.x += basis.forward.x * speed;
        player->position.y += basis.forward.y * speed;
        player->position.z += basis.forward.z * speed;
    }
    if (IsKeyDown(KEY_S)) {
        player->position.x -= basis.forward.x * speed;
        player->position.y -= basis.forward.y * speed;
        player->position.z -= basis.forward.z * speed;
    }
    if (IsKeyDown(KEY_A)) {
        player->position.x += basis.right.x * speed;
        player->position.z += basis.right.z * speed;
    }
    if (IsKeyDown(KEY_D)) {
        player->position.x -= basis.right.x * speed;
        player->position.z -= basis.right.z * speed;
    }
    if (IsKeyDown(KEY_SPACE)) {
        player->position.y += speed;
    }
}

void PlayerSyncCamera(const Player *player, Camera3D *camera, Vector3 *outDirection)
{
    FpsBasis basis = FpsLookGetBasis(&player->look);

    camera->position = player->position;
    camera->target = Vector3Add(player->position, basis.forward);
    camera->up = basis.up;

    if (outDirection) *outDirection = basis.forward;
}

Vector2Int PlayerChunkCoord(const Player *player)
{
    return (Vector2Int){
        (int)floorf(player->position.x / CHUNK_SIZE),
        (int)floorf(player->position.z / CHUNK_SIZE)
    };
}

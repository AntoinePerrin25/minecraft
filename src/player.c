#include "player.h"

#include "config.h"
#include "raymath.h"

#include <math.h>

void PlayerInit(Player *player)
{
    player->position = (Vector3){ 0.0f, 66.0f, 0.0f };
    player->velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
    player->yaw = 0.0f;
    player->pitch = 0.0f;
    player->id = 0;
}

void PlayerUpdate(Player *player, float deltaTime)
{
    float mouseX = GetMouseDelta().x * 0.2f;
    float mouseY = GetMouseDelta().y * 0.2f;

    player->yaw -= mouseX;
    player->pitch -= mouseY;

    if (player->pitch > 89.9999f) player->pitch = 89.9999f;
    if (player->pitch < -89.9999f) player->pitch = -89.9999f;

    Vector3 direction = {
        cosf(player->pitch * DEG2RAD) * sinf(player->yaw * DEG2RAD),
        sinf(player->pitch * DEG2RAD),
        cosf(player->pitch * DEG2RAD) * cosf(player->yaw * DEG2RAD)
    };

    float speed = 10.0f * deltaTime;
    if (IsKeyDown(KEY_LEFT_SHIFT)) speed *= 2.5f;

    if (IsKeyDown(KEY_W)) {
        player->position.x += direction.x * speed;
        player->position.y += direction.y * speed;
        player->position.z += direction.z * speed;
    }
    if (IsKeyDown(KEY_S)) {
        player->position.x -= direction.x * speed;
        player->position.y -= direction.y * speed;
        player->position.z -= direction.z * speed;
    }

    Vector3 right = (Vector3){ direction.z, 0.0f, -direction.x };
    if (IsKeyDown(KEY_A)) {
        player->position.x += right.x * speed;
        player->position.z += right.z * speed;
    }
    if (IsKeyDown(KEY_D)) {
        player->position.x -= right.x * speed;
        player->position.z -= right.z * speed;
    }
    if (IsKeyDown(KEY_SPACE)) {
        player->position.y += speed;
    }
}

void PlayerSyncCamera(const Player *player, Camera3D *camera, Vector3 *outDirection)
{
    Vector3 direction = {
        cosf(player->pitch * DEG2RAD) * sinf(player->yaw * DEG2RAD),
        sinf(player->pitch * DEG2RAD),
        cosf(player->pitch * DEG2RAD) * cosf(player->yaw * DEG2RAD)
    };

    camera->position = player->position;
    camera->target = Vector3Add(camera->position, direction);

    if (outDirection) *outDirection = direction;
}



Vector2Int PlayerChunkCoord(const Player *player)
{
    return (Vector2Int){
        (int)floorf(player->position.x / CHUNK_SIZE),
        (int)floorf(player->position.z / CHUNK_SIZE)
    };
}

#include "player.h"

#include "camera_fps.h"
#include "config.h"
#include "raymath.h"

#include <math.h>

void PlayerInit(Player *player)
{
    player->position     = (Vector3){ 0.0f, 66.0f, 0.0f };
    player->velocity     = (Vector3){ 0.0f, 0.0f,  0.0f };
    player->acceleration = (Vector3){ 0.0f, 0.0f,  0.0f };
    FpsLookInit(&player->look);
    player->mode = GAMEMODE_SPECTATOR;
    player->id = 0;
}

void PlayerUpdateSpectator(Player *player, FpsBasis basis, float speed)
{
    if (IsKeyDown(KEY_Q)) speed *= 2.5f;

    if (IsKeyDown(KEY_W)) {
        player->position = Vector3Add(player->position, Vector3Scale(basis.forward, speed));
    }
    if (IsKeyDown(KEY_S)) {
        player->position = Vector3Subtract(player->position, Vector3Scale(basis.forward, speed));
    }
    bool a = IsKeyDown(KEY_A);
    bool d = IsKeyDown(KEY_D);
    if (a && d) {
        // do nothing
    } else if (IsKeyDown(KEY_A)) {
        player->position.x += basis.right.x * speed;
        player->position.z += basis.right.z * speed;
    } else if (IsKeyDown(KEY_D)) {
        player->position.x -= basis.right.x * speed;
        player->position.z -= basis.right.z * speed;
    }

    const bool shift = IsKeyDown(KEY_LEFT_SHIFT);
    const bool space = IsKeyDown(KEY_SPACE);
    if (shift && space) {
        // do nothing
    } else if (shift) {
        player->position.y -= speed;
    } else if (space) {
        player->position.y += speed;
    }
}

void PlayerUpdateSurvival(Player *player, FpsBasis basis, float speed)
{
    return;
}

inline void PlayerSetGamemode(Player *player, Gamemode mode)
{
    player->mode = mode;
}

void PlayerUpdate(Player *player, float deltaTime)
{
    float mouseX = GetMouseDelta().x * 0.2f;
    float mouseY = GetMouseDelta().y * 0.2f;

    FpsLookApplyMouse(&player->look, mouseX, mouseY);
    FpsBasis basis = FpsLookGetBasis(&player->look);

    float speed = 10.0f * deltaTime;

    if (player->mode == GAMEMODE_SPECTATOR) {
        PlayerUpdateSpectator(player, basis, speed);
    }
    else if (player->mode == GAMEMODE_SURVIVAL) {
        PlayerUpdateSurvival(player, basis, speed);
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

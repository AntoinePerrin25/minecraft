#ifndef MC_PLAYER_H
#define MC_PLAYER_H

#include "camera_fps.h"
#include "types.h"
#include "raylib.h"

typedef enum Gamemode {
    GAMEMODE_SPECTATOR,
    GAMEMODE_SURVIVAL,
} Gamemode;

typedef struct Player {
    Vector3 position;
    Vector3 velocity;
    FpsLook look;
    Gamemode mode;
    int id;
} Player;

void PlayerInit(Player *player);
void PlayerUpdate(Player *player, float deltaTime);
void PlayerSyncCamera(const Player *player, Camera3D *camera, Vector3 *outDirection);
Vector2Int PlayerChunkCoord(const Player *player);

#endif

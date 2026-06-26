#ifndef MC_PLAYER_H
#define MC_PLAYER_H

#include "camera_fps.h"
#include "types.h"
#include "raylib.h"

#define MAX_WALK_SPEED 1.0f
#define MAX_RUN_SPEED  2.5f
#define GRAVITY        9.81f

typedef enum Gamemode {
    GAMEMODE_SPECTATOR,
    GAMEMODE_SURVIVAL,
} Gamemode;

typedef struct Player {
    Vector3 position;
    Vector3 velocity;
    Vector3 acceleration;
    FpsLook look;
    Gamemode mode;
    int id;
} Player;

void PlayerInit(Player *player);
void PlayerUpdateSpectator(Player *player, FpsBasis basis, float speed);
void PlayerUpdateSurvival(Player *player, FpsBasis basis, float speed);
inline void PlayerSetGamemode(Player *player, Gamemode mode);
void PlayerUpdate(Player *player, float deltaTime);
void PlayerSyncCamera(const Player *player, Camera3D *camera, Vector3 *outDirection);
Vector2Int PlayerChunkCoord(const Player *player);

#endif

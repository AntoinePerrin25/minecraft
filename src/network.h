#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>

#define MAX_PLAYERS 32
#define SERVER_PORT 7777
#define SERVER_TICK_RATE 50  // 20ms entre chaque mise à jour
#define INTERPOLATION_DELAY 100  // 100ms de délai pour l'interpolation
#define SERVER_PRINT_DEBUG_DELAY 2000  // 2s entre chaque message de débogage
typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    Vec3 position;
    Vec3 velocity;
    float yaw;
    float pitch;
    bool connected;
    int id;
} NetworkPlayer;

typedef enum {
    PACKET_CONNECT,
    PACKET_DISCONNECT,
    PACKET_PLAYER_STATE,
    PACKET_WORLD_STATE
} PacketType;

typedef struct {
    PacketType type;
    NetworkPlayer player;
} Packet;

#endif // NETWORK_H
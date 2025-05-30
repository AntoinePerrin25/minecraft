#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include <stdint.h>

#include "chunk_mesh.h"

// Constantes réseau
#define MAX_PLAYERS 32
#define SERVER_PORT 7777
#define SERVER_TICK_RATE 50  // 20ms entre chaque mise à jour
#define INTERPOLATION_DELAY 100  // 100ms de délai pour l'interpolation
#define SERVER_PRINT_DEBUG_DELAY 2000  // 2s entre chaque message de débogage

// Constantes du monde
#define CHUNK_SIZE 16
#define WORLD_HEIGHT 256
#define RENDER_DISTANCE 3
#define MAX_LOADED_CHUNKS 100
#define CHUNK_LOAD_DISTANCE 3

typedef struct {
    float x, y, z;
} Vec3;


typedef struct {
    int x, y, z;
    BlockData block;
} BlockUpdate;

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
    PACKET_WORLD_STATE,
    PACKET_CHUNK_REQUEST,   // Client -> Serveur
    PACKET_CHUNK_DATA,      // Serveur -> Client
    PACKET_BLOCK_UPDATE     // Bidirectionnel
} PacketType;

typedef struct {
    PacketType type;
    union {
        NetworkPlayer player;
        ChunkData chunk;
        struct {
            int chunkX, chunkZ;  // Pour PACKET_CHUNK_REQUEST
        };
        BlockUpdate blockUpdate;
    };
} Packet;



#endif // NETWORK_H
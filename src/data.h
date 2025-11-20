#ifndef DATA_H
#define DATA_H
#include "raylib.h"
#include <stdint.h>


#define CHUNK_SIZE 16
#define WORLD_HEIGHT 128
#define RENDER_DISTANCE 4

#define WINDOWS_WIDTH 800
#define WINDOWS_HEIGHT 600


typedef struct Vector2Int {
    int x;
    int z;
} Vector2Int;

typedef struct Vector3Int {
    int x;
    int y;
    int z;
} Vector3Int;

typedef struct BlockInWorld {
    Vector3Int blockCoord;
    Vector2Int chunkCoord;
} BlockInWorld;

Vector3Int Coords_world2Block(Vector3 worldPos);

typedef struct {
    Vector3 position;
    Vector3 velocity;
    float yaw;
    float pitch;
    int id;
} Player;

// Block type enumeration 2^9 = 512 possible block types
typedef enum
{
    BLOCK_NONE,
    BLOCK_AIR,
    BLOCK_BEDROCK,
    BLOCK_DIRT,
    BLOCK_GRASS,
    BLOCK_STONE,
    BLOCK_WATER,
    BLOCK_SAND,
    BLOCK_WOOD,
    BLOCK_NULL,
    BLOCK_BREAKING
} BlockType;

typedef struct __attribute__((packed, aligned(1))) BlockData
{
    uint16_t Type       : 9;
    uint16_t lightLevel : 4;
    uint16_t gravity    : 1;
    uint16_t solid      : 1;
    uint16_t visible    : 1;
} BlockData;

typedef struct __attribute__((packed)) ChunkData
{
    int8_t ChunkHeight;
    BlockData blocks[16][128][16];
} ChunkData;

typedef struct ChunkRenderData {
    unsigned int vao; // using rlgl / raylib Mesh under the hood
    unsigned int vbo;
    unsigned int ibo;
    int indexCount;
    int vertexCount;
    void *cpuVertices;
    void *cpuIndices;
    int needsRemesh;
    int meshing;
    int meshReady;
    float aabbMin[3];
    float aabbMax[3];
    void *user; // reserved
    int hasMesh;
    Mesh mesh;
} ChunkRenderData;

typedef struct {
    int x;
    int z;
    ChunkData data;
    ChunkRenderData render;
} Chunk;

BlockData createBlock(BlockType type);
void generateChunk(Chunk *chunk, int chunkX, int chunkZ);
BlockData getBlockAt(Chunk *chunks, int worldX, int worldY, int worldZ);
int isBlockExposed(Chunk *chunks, int x, int y, int z);

#endif
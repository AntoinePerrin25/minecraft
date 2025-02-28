#include <stdint.h>

#include "raylib.h"

#define MAX_LIGHT_LEVEL 2**4
#define CHUNK_SIZE 16
#define WORLD_HEIGHT 256
#define RENDER_DISTANCE 2
#define LIGHT_LEVEL_MASK     0xF000
#define NLIGHT_LEVEL_MASK    (~LIGHT_LEVEL_MASK)
#define GRAVITY_ID_MASK      0x0800
#define NGRAVITY_ID_MASK     (~GRAVITY_ID_MASK)
#define SOLID_ID_MASK        0x0400
#define NSOLID_ID_MASK       (~SOLID_ID_MASK)
#define TRANSPARENT_ID_MASK  0x0200
#define NTRANSPARENT_ID_MASK (~TRANSPARENT_ID_MASK)
#define BLOCK_ID_MASK        0x01FF
#define NBLOCK_ID_MASK       (~BLOCK_ID_MASK)

// 12 bits enum
typedef enum
{
    BLOCK_AIR= 0,
    BLOCK_GRASS = 1,
    BLOCK_DIRT = 2,
    BLOCK_STONE = 3,
    BLOCK_BEDROCK = 4,
    BLOCK_WATER = 5,
    BLOCK_SAND = 6,
    BLOCK_WOOD = 7,
} BlockType;


typedef uint16_t BlockData;
// 2 bytes : LLLL'GSTB'BBBB'BBBB
// L : 4 (Light)
// G : 1 (Gravity)
// S : 1 (Solid)
// T : 1 (Transparent)
// B : 9 (Block)

static inline BlockData createBlock(BlockType id, uint8_t lightLevel, uint8_t gravity, uint8_t solid, uint8_t transparent) {
    uint16_t result = id & BLOCK_ID_MASK;
    if (gravity) result |= GRAVITY_ID_MASK;
    if (solid) result |= SOLID_ID_MASK;
    if (transparent) result |= 0x0200; // Transparent bit (bit 9)
    result |= ((lightLevel & 0x0F) << 12);
    return result;
}

static inline BlockType getBlockID(BlockData block) {
    return (BlockType)(block & BLOCK_ID_MASK);
}

static inline uint8_t getLightLevel(BlockData block) {
    return (block >> 12) & 0x0F;
}

static inline uint8_t isGravityEnabled(BlockData block) {
    return (block & GRAVITY_ID_MASK) != 0;
}

static inline uint8_t isSolid(BlockData block) {
    return (block & SOLID_ID_MASK) != 0;
}

static inline uint8_t isTransparent(BlockData block) {
    return (block & TRANSPARENT_ID_MASK) != 0; // Check transparent bit
}

static inline BlockData setBlockID(BlockData block, BlockType id) {
    return (block & NBLOCK_ID_MASK) | (id & BLOCK_ID_MASK);
}

static inline BlockData setLightLevel(BlockData block, uint8_t lightLevel) {
    return (block & NLIGHT_LEVEL_MASK) | ((lightLevel & 0x0F) << 12);
}

static inline BlockData setGravity(BlockData block, uint8_t gravity) {
    return gravity ? (block | GRAVITY_ID_MASK) : (block & NGRAVITY_ID_MASK);
}

static inline BlockData setSolid(BlockData block, uint8_t solid) {
    return solid ? (block | SOLID_ID_MASK) : (block & NSOLID_ID_MASK);
}

static inline BlockData setTransparent(BlockData block, uint8_t transparent) {
    return transparent ? (block | TRANSPARENT_ID_MASK) : (block & NTRANSPARENT_ID_MASK);
}

typedef struct __attribute__((packed)) ChunkLayer
{
    BlockData blocks[CHUNK_SIZE][CHUNK_SIZE];
} ChunkLayer;


typedef struct __attribute__((packed)) ChunkVertical
{
    ChunkLayer layers[CHUNK_SIZE];
    unsigned int y;
    unsigned int isOnlyBlockType : 1;
    BlockType blockType : 3;

} ChunkVertical;

typedef struct __attribute__((packed)) ChunkData
{
    ChunkVertical* verticals[WORLD_HEIGHT];
    unsigned int isLoaded : 1;
    int x, z;
} ChunkData;


typedef struct ChunkMesh
{
    Mesh mesh;
    Vector3 *vertices;
    Vector3 *normals;
    Color *colors;
    unsigned short *indices;
    int vertexCount;
    int indexCount;
    int capacity;
} ChunkMesh;


typedef struct __attribute__((packed)) ClientChunk
{
    ChunkData *data;
    ChunkMesh *mesh;
    unsigned int loaded : 1;
} ClientChunk;


typedef struct ChunkManager
{
    ClientChunk **chunks; // Tableau de pointeurs vers les chunks
    int count;
    int capacity;
} ChunkManager;

typedef struct
{
    Vector3 position;
    Vector3 velocity;
    float yaw;
    float pitch;
    int id;
} Player;

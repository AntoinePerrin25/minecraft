#include <stdint.h>

#include "../include/raylib.h"

#define MAX_LIGHT_LEVEL 16
#define CHUNK_SIZE 16
#define CHUNK_SIZE2 256
#define CHUNK_SIZE3 4096

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

typedef struct __attribute__((packed)) BlockUpdate
{
    int x;
    int y;
    int z;
    BlockData block;
} BlockUpdate;



typedef struct __attribute__((packed)) BlockData
{
    BlockType Type :9;
    uint8_t lightLevel : 4;
    uint8_t gravity :1;
    uint8_t solid :1;
    uint8_t transparent :1;
} BlockData;

typedef struct __attribute__((packed)) ChunkVertical
{
    BlockData layers[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
    unsigned int y;
    unsigned int isOnlyBlockType : 1;
    BlockType blockType : 3;

} ChunkVertical;

typedef struct __attribute__((packed)) ChunkData
{
    ChunkVertical* verticals[WORLD_HEIGHT];
    // 16 vertical chunks (16*16*16) = 4096 blocks
} ChunkData;


typedef struct __attribute__((packed)) ClientChunk
{
    ChunkData *data;
    Mesh *mesh;
    unsigned int loaded : 1;
    int x;
    int z;
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



// Chunk manager initialization and cleanup
ChunkManager InitChunkManager(int initialCapacity);
void FreeChunkManager(ChunkManager manager);

// Chunk operations
ClientChunk* CreateClientChunk(int x, int z);
void FreeClientChunk(ClientChunk* chunk);
ClientChunk* GetChunk(ChunkManager* manager, int x, int z);
void AddChunk(ChunkManager* manager, ClientChunk* chunk);
void RemoveChunk(ChunkManager* manager, int x, int z);
Vector2 worldToChunkCoords(Vector3 worldPos);
void unloadDistantChunks(ChunkManager* manager, Vector3 playerPos);

// Chunk vertical operations
ChunkVertical* CreateChunkVertical(unsigned int y);
void FreeChunkVertical(ChunkVertical* vertical);

// Block operations
BlockData GetBlock(ChunkManager* manager, int x, int y, int z);
void SetBlock(ChunkManager* manager, int x, int y, int z, BlockData block);
void SetBlockType(ChunkManager* manager, int x, int y, int z, BlockType type);

// Mesh generation
void GenerateChunkMesh(ClientChunk* chunk);
void UpdateChunkMesh(ClientChunk* chunk);
void RenderChunks(ChunkManager* manager, Player* player);

// Light propagation
void PropagateLight(ChunkManager* manager, int x, int y, int z, uint8_t lightLevel);
void UpdateLightLevels(ChunkManager* manager, int x, int z);
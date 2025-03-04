#ifndef CHUNK_MANAGER_H
#define CHUNK_MANAGER_H

#include <stdint.h>

#include <raylib/raylib.h>

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

// Vector of 2 ints
typedef struct Vector2Int {
    int x;                // Vector x component
    int y;                // Vector y component
} Vector2Int;

// Vector of 3 ints
typedef struct Vector3Int {
    int x;                // Vector x component
    int y;                // Vector y component
    int z;                // Vector z component
} Vector3Int;


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
} BlockType;

/**
 * @brief Block data structure containing block type and properties on 16 bits
 * @param Type Block type identifier (9 bits)
 * @param lightLevel Light level of the block (4 bits)
 * @param gravity Whether the block is affected by gravity (1 bit)
 * @param solid Whether the block is solid (1 bit)
 * @param visible Whether the block is visible (1 bit)
 */
typedef struct __attribute__((packed, aligned(1))) BlockData
{
    uint16_t Type       : 9;
    uint16_t lightLevel : 4;
    uint16_t gravity    : 1;
    uint16_t solid      : 1;
    uint16_t visible    : 1;
} BlockData;

/**
 * @brief Structure representing a block update event
 * @param x X coordinate of the block
 * @param y Y coordinate of the block
 * @param z Z coordinate of the block
 * @param block Block data for the update
 */

typedef struct BlockUpdate
{
    Vector3Int blockpos;
    BlockData block;
} BlockUpdate;


/**
 * @brief Vertical chunk structure containing 16x16x16 blocks
 * @param blocks 3D array of block data
 * @param verticaly Vertical position of the chunk (4 bits)
 * @param isOnlyBlockType Flag indicating if chunk contains only one block type (1 bit)
 * @param blockType Type of block if chunk contains only one type (9 bits)
 */
typedef struct __attribute__((packed)) ChunkVertical
{
    BlockData blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
} ChunkVertical;

/**
 * @brief Chunk data structure containing 16 vertical chunks
 * @param *verticals Array of pointers to vertical chunks
 */
typedef struct ChunkData
{
    ChunkVertical* verticals[CHUNK_SIZE];
    BlockType blockType[CHUNK_SIZE];
} ChunkData;

typedef struct FullChunk
{
    BlockData blocks[CHUNK_SIZE2][CHUNK_SIZE][CHUNK_SIZE];  // 256 * (16 * 16)
} FullChunk;

/**
 * @brief Structure representing an update to a chunk's state
 * @param chunkPos Position of the chunk being updated
 * @param chunk Data of the chunk being updated
 */
typedef struct ChunkUpdate
{
    Vector3Int chunkPos;
    ChunkData chunk;
} ChunkUpdate;


/**
 * @brief Client-side chunk structure containing rendering data
 * @param *data Pointer to chunk data
 * @param *mesh Pointer to chunk mesh
 * @param loaded Flag indicating if chunk is loaded
 * @param x X coordinate of chunk in world
 * @param z Z coordinate of chunk in world
 */
typedef struct __attribute__((packed)) ClientChunk
{
    ChunkData data;
    Mesh mesh;
    int x;
    int z;
    unsigned int loaded : 1;
} ClientChunk;

/**
 * @brief Chunk manager structure for handling multiple chunks
 * @param **chunks Array of pointers to client chunks
 * @param count Number of currently allocated chunks
 * @param capacity Maximum number of chunks that can be stored
 */
typedef struct ChunkManager
{
    int capacity;
    int count;
    ClientChunk *chunks[1]; // Tableau de pointeurs vers les chunks
} ChunkManager;

/**
 * @brief Player structure containing position and orientation data
 * @param position Player's position in world
 * @param velocity Player's velocity vector
 * @param yaw Player's horizontal rotation
 * @param pitch Player's vertical rotation
 * @param id Player's unique identifier
 */
typedef struct
{
    Vector3 position;
    Vector3 velocity;
    float yaw;
    float pitch;
    int id;
} Player;

// Chunk manager initialization and cleanup
ChunkManager* InitChunkManager   (int initialCapacity);
void FreeChunkManager           (ChunkManager* manager);

// Chunk operations
ClientChunk* CreateClientChunk  (                       int x, int z);
ClientChunk* GetChunk           (ChunkManager* manager, int x, int z);
void RemoveChunk                (ChunkManager* manager, int index);
void FreeClientChunk            (                       ClientChunk* chunk);
void AddChunk                   (ChunkManager* manager, ClientChunk* chunk);
void unloadDistantChunks        (ChunkManager* manager, const Vector3* playerPos);
Vector3Int worldToChunkCoords   (                       const Vector3* worldPos);
ChunkData CompressChunk         (const FullChunk* fullChunk);  // New function

// Chunk vertical operations
ChunkVertical* CreateChunkVertical(void);

// Block operations
BlockData* GetBlock      (const ChunkManager* manager, int x, int y, int z);
void SetBlock                  (ChunkManager* manager, int x, int y, int z, BlockData block);

// Mesh generation
void GenerateChunkMesh          (ClientChunk* chunk);
void UpdateChunkMesh            (ClientChunk* chunk);
void RenderChunks               (ChunkManager* manager, Player* player);

// Light propagation
void PropagateLight             (ChunkManager* manager, int x, int y, int z, uint8_t lightLevel);
void UpdateLightLevels          (ChunkManager* manager, int x, int z);

// Debugging
void printChunkLoaded    (const ChunkManager* manager);

#endif // CHUNK_MANAGER_H
#include "raylib.h"

#define CHUNK_SIZE 16
#define WORLD_HEIGHT 256
#define RENDER_DISTANCE 2

// 3 bits enum
typedef enum
{
    BLOCK_AIR = 0,
    BLOCK_GRASS = 1,
    BLOCK_DIRT = 2,
    BLOCK_STONE = 3,
    BLOCK_BEDROCK = 4,
    BLOCK_WATER = 5,
    BLOCK_SAND = 6,
    BLOCK_WOOD = 7
} BlockType;

// Bitfield for block types

typedef struct __attribute__((packed)) ChunkBit
{    
    BlockType blockType : 3;
} ChunkBit;

typedef struct __attribute__((packed)) ChunkLayer
{
    ChunkBit blocks[CHUNK_SIZE * CHUNK_SIZE];  // Flatten the 2D array into 1D
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
    unsigned int activeVerticals : 8;  // Track number of non-empty verticals
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

typedef struct ChunkManager
{
    ClientChunk **chunks; // Tableau de pointeurs vers les chunks
    int count;
    int capacity;
    } ChunkManager;

typedef struct __attribute__((packed)) ClientChunk
{
    ChunkData *data;
    ChunkMesh *mesh;
    unsigned int loaded : 1;
} ClientChunk;


typedef struct
{
    Vector3 position;
    Vector3 velocity;
    float yaw;
    float pitch;
    int id;
} Player;

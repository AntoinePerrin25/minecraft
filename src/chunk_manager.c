#include <math.h>

#include "../nob.h"

#include "chunk_manager.h"
#include "raylib.h"

#define NOB_LEVEL_DEF NOB_INFO


// Chunk manager initialization and cleanup
ChunkManager *InitChunkManager(int initialCapacity)
{
    ChunkManager* manager = calloc(1, sizeof(*manager) + initialCapacity*(sizeof(ClientChunk*)));
    if (!manager) {
        nob_log(NOB_ERROR, "Failed to allocate chunk manager");
    }
    else {
        nob_log(NOB_LEVEL_DEF, "Allocated chunk manager chunks\n");
        // Count already to 0
        manager->capacity = initialCapacity;
    }
    return manager;
}

void FreeChunkManager(ChunkManager* manager)
{
    for (int i = 0; i < manager->count; i++)
    {
        if (manager->chunks[i])
            FreeClientChunk(manager->chunks[i]);
    }
    free(manager);
    nob_log(NOB_LEVEL_DEF, "Freed chunk manager\n");
}

// Chunk operations
// Allocate memory for ClientChunk
ClientChunk* CreateClientChunk(int x, int z)
{
    ClientChunk* chunk = calloc(1, sizeof(ClientChunk));
    if (!chunk) return NULL;

    chunk->x = x;
    chunk->z = z;

    nob_log(NOB_LEVEL_DEF, "Created chunk at (%d, %d)\n", x, z);

    return chunk;
}

void FreeClientChunk(ClientChunk* chunk)
{
    if (!chunk)
    {
        nob_log(NOB_ERROR, "Unable to free chunk: Chunk is NULL\n");
        return;
    }
    
    int x = chunk->x;
    int z = chunk->z;

    nob_log(NOB_LEVEL_DEF, "Freeing chunk at (%d, %d)\n", x, z);
    
    if (chunk->mesh.initialized) {
        nob_log(NOB_WARNING, "FreeClientChunk: Unloading mesh\n");
        UnloadMesh(chunk->mesh.mesh);
    }
    
    free(chunk);
    nob_log(NOB_LEVEL_DEF, "Freed chunk at (%d, %d)\n", x, z);
}


ClientChunk* GetChunk(ChunkManager* manager, int x, int z)
{
    nob_log(NOB_LEVEL_DEF, "Retrieving chunk at (%d, %d)\n", x, z);
    for (int i = 0; i < manager->capacity; i++)
    {
        if (manager->chunks[i]          &&
            manager->chunks[i]->loaded  &&
            manager->chunks[i]->x == x  &&
            manager->chunks[i]->z == z)
        {
            
            nob_log(NOB_LEVEL_DEF, "Chunk retrieved at (%d, %d)\n", x, z);
            return manager->chunks[i];
        }
    }
    return NULL;
}

void AddChunk(ChunkManager* manager, ClientChunk* chunk)
{
    if (!chunk)
    {
        nob_log(NOB_ERROR, "Unable to add chunk: Chunk is NULL");
        return;
    }
    if (manager->count >= manager->capacity) 
    {
        nob_log(NOB_ERROR, "Unable to add chunk: Chunk manager is full");
        return;
    }

    // Add chunk at the next available index (count)
    manager->chunks[manager->count] = chunk;
    manager->count++;
    
    nob_log(NOB_INFO, "Chunk added at (%d, %d)\n", chunk->x, chunk->z);
}

void RemoveChunk(ChunkManager* manager, int index)
{
    ClientChunk* chunk = manager->chunks[index];
    int x = chunk->x;
    int z = chunk->z;
    nob_log(NOB_LEVEL_DEF, "Removing chunk at (%d, %d)\n", chunk->x, chunk->z);
 
    FreeClientChunk(manager->chunks[index]);
    manager->chunks[index] = NULL;
    manager->count--;
    nob_log(NOB_LEVEL_DEF, "Chunk at (%d, %d) successfully removed\n", x, z);
    return;
}

Vector3Int worldToChunkCoords(const Vector3* worldPos)
{
    return (Vector3Int) {floor(worldPos->x / CHUNK_SIZE),
                         floor(worldPos->y / CHUNK_SIZE),
                         floor(worldPos->z / CHUNK_SIZE)};
}

// Inline functions to convert coordinates to index and back
static inline int BlockIndex(int x, int y, int z) {
    return y * CHUNK_SIZE2 + x * CHUNK_SIZE + z;
}

static inline void IndexToCoords(int index, int* x, int* y, int* z) {
    *y = index / CHUNK_SIZE2;
    *x = (index % CHUNK_SIZE2) / CHUNK_SIZE;
    *z = index % CHUNK_SIZE;
}

void unloadDistantChunks(ChunkManager* manager, const Vector3* playerPos)
{
    Vector3Int playerChunk = worldToChunkCoords(playerPos);
    
    for (int index = 0; index < manager->capacity; index++)
    {
        if (!manager->chunks[index] || !manager->chunks[index]->loaded) continue;
        
        int dx = abs(manager->chunks[index]->x - playerChunk.x);
        int dz = abs(manager->chunks[index]->z - playerChunk.z);
        
        if (dx > RENDER_DISTANCE || dz > RENDER_DISTANCE)
        {
            // Free the resources of the chunk
            RemoveChunk(manager, index);
        }
    }
    nob_log(NOB_LEVEL_DEF, "Unloaded Chunks at %d, %d\n", playerChunk.x, playerChunk.z);
}

// Helper function to get block type from a chunk
BlockType GetBlockType(const FullChunk* chunk, int x, int y, int z)
{
    // Check bounds
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= WORLD_HEIGHT || z < 0 || z >= CHUNK_SIZE)
        return BLOCK_AIR; // Out of bounds is treated as air
    
    return chunk->blocks[y][x][z].Type;
}

// Block operations
BlockData* GetBlock(const ChunkManager* manager, int x, int y, int z)
{
    Vector3Int chunkpos = worldToChunkCoords(&(Vector3) {x, y ,z});
    
    for (int i = 0; i < manager->capacity; i++)
    {
        if (manager->chunks[i] && manager->chunks[i]->x == chunkpos.x && manager->chunks[i]->z == chunkpos.z)
        {
            // Get local coordinates within the chunk
            int localX = x % CHUNK_SIZE;
            if (localX < 0) localX += CHUNK_SIZE;
            
            int localY = y;  // Y is already global
            
            int localZ = z % CHUNK_SIZE;
            if (localZ < 0) localZ += CHUNK_SIZE;
            
            return &manager->chunks[i]->chunk.blocks[localY][localX][localZ];
        }
    }
    return NULL;
}

void SetBlock(ChunkManager* manager, int x, int y, int z, BlockData block)
{
    BlockData* blockData = GetBlock(manager, x, y, z);
    if (blockData)
    {
        *blockData = block;
    }
}

void FillBlocks(ChunkManager* manager, int x, int y, int z, int width, int height, int depth, BlockData block)
{
    NOB_TODO("FillBlocks");
    NOB_UNUSED(manager);
    NOB_UNUSED(x);
    NOB_UNUSED(y);
    NOB_UNUSED(z);
    NOB_UNUSED(width);
    NOB_UNUSED(height);
    NOB_UNUSED(depth);
    NOB_UNUSED(block);
}


void printChunkLoaded(const ChunkManager* manager)
{
    printf("=====================\n");
    printf("Loaded Chunks:\n");
    for (int i = 0; i < manager->capacity; i++)
    {
        if (manager->chunks[i])
        {
            printf("\tChunk at (%2d, %2d) is loaded @%p\n", manager->chunks[i]->x, manager->chunks[i]->z, manager->chunks[i]);
        }
    }
    printf("=====================\n");
}

void printFullChunk(FullChunk fullChunk, FILE* file)
{
    fprintf(file, "=====================\n");
    fprintf(file, "Full Chunk:\n");
    for (int y = CHUNK_SIZE2-1; y >= 0; y--)
    {
        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            fprintf(file, "%d ", fullChunk.blocks[y][x][7].Type);
        }
        fprintf(file, "\n");
    }
    fprintf(file, "=====================\n");
}

// Compare deux FullChunk pour vérifier s'ils sont identiques
int AreFullChunksEqual(const FullChunk* chunk1, const FullChunk* chunk2)
{
    if (!chunk1 || !chunk2) return 0;
    
    for (int y = 0; y < CHUNK_SIZE2; y++)
    {
        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            for (int z = 0; z < CHUNK_SIZE; z++)
            {
                BlockData block1 = chunk1->blocks[y][x][z];
                BlockData block2 = chunk2->blocks[y][x][z];
                
                // Compare all properties of BlockData
                if (block1.Type != block2.Type || 
                    block1.lightLevel != block2.lightLevel ||
                    block1.gravity != block2.gravity ||
                    block1.solid != block2.solid ||
                    block1.visible != block2.visible)
                {
                    return 0;  // Not equal
                }
            }
        }
    }
    
    return 1;  // Equal
}
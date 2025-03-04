#include <math.h>


#include "chunk_manager.h"
#include "raylib.h"

#include "../nob.h"
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
    
    nob_log(NOB_WARNING, "FreeClientChunk : Mesh Need to be unloaded later\n");
    UnloadMesh(chunk->mesh);
    
    for(int y = 0; y < CHUNK_SIZE; y++)
    {
        free(chunk->data.verticals[y]);
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

// Get block from the new linear layout
static inline BlockData* GetBlockFromVertical(ChunkVertical* vertical, int x, int y, int z) {
    if (!vertical) return NULL;
    return &vertical->blocks[x][y][z];
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

// Chunk vertical operations
ChunkVertical* CreateChunkVertical(void)
{
    return calloc(1, sizeof(ChunkVertical));
}


// Block operations
BlockData* GetBlock(const ChunkManager* manager, int x, int y, int z)
{
    Vector3Int chunkpos = worldToChunkCoords(&(Vector3) {x, y ,z});
    
    for (int i = 0; i < manager->capacity; i++)
    {
        if (manager->chunks[i] && manager->chunks[i]->x == chunkpos.x && manager->chunks[i]->z == chunkpos.z)
        {
            ChunkVertical *vertical = manager->chunks[i]->data.verticals[chunkpos.y];
            if (vertical)
            {
                return &vertical->blocks[x % CHUNK_SIZE][y % CHUNK_SIZE][z % CHUNK_SIZE];
            }
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

// Compress a FullChunk into a ChunkData structure
ChunkData CompressChunk(const FullChunk* fullChunk)
{
    ChunkData chunkData = {0};  // Initialize to zeros
    
    // Process each vertical section
    for (int section = 0; section < CHUNK_SIZE; section++)
    {
        int startY = section * CHUNK_SIZE;
        
        // Check if this section has blocks (not all air)
        bool hasBlocks = false;
        BlockType firstType = fullChunk->blocks[startY][0][0].Type;
        bool allSameType = true;
        
        // First pass: Check if all blocks are the same type
        for (int y = 0; y < CHUNK_SIZE && allSameType; y++)
        {
            int worldY = startY + y;
            if (worldY >= CHUNK_SIZE2) break;
            
            for (int x = 0; x < CHUNK_SIZE && allSameType; x++)
            {
                for (int z = 0; z < CHUNK_SIZE && allSameType; z++)
                {
                    BlockType currentType = fullChunk->blocks[worldY][x][z].Type;
                    
                    if (currentType != BLOCK_AIR)
                        hasBlocks = true;
                    
                    if (currentType != firstType)
                        allSameType = false;
                }
            }
        }
        
        // If all blocks are the same type
        if (allSameType)
        {
            // Store the block type and don't allocate a vertical section
            chunkData.blockType[section] = firstType;

            free(chunkData.verticals[section]);
            chunkData.verticals[section] = NULL;
            
            // If it's all air, no need to further process this section
            if (firstType == BLOCK_AIR || !hasBlocks)
                continue;
        }
        else
        {
            // Blocks are not all the same type, so we need to allocate a vertical section
            chunkData.blockType[section] = BLOCK_NONE;
            chunkData.verticals[section] = CreateChunkVertical();
            
            if (!chunkData.verticals[section])
            {
                nob_log(NOB_ERROR, "Failed to allocate chunk vertical section %d", section);
                continue;
            }
            
            // Copy blocks from the full chunk to this vertical section
            for (int y = 0; y < CHUNK_SIZE; y++)
            {
                int worldY = startY + y;
                if (worldY >= CHUNK_SIZE2) break;
                
                for (int x = 0; x < CHUNK_SIZE; x++)
                {
                    for (int z = 0; z < CHUNK_SIZE; z++)
                    {
                        chunkData.verticals[section]->blocks[y][x][z] = fullChunk->blocks[worldY][x][z];
                    }
                }
            }
        }
    }
    nob_log(NOB_LEVEL_DEF, "Compressed chunk at \n");

    return chunkData;
}
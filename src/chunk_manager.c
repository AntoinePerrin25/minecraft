#include <math.h>

#define NOB_IMPLEMENTATION
#include "../nob.h"
#include "chunk_manager.h"

#include "raylib.h"


// Chunk manager initialization and cleanup
ChunkManager *InitChunkManager(int initialCapacity)
{
    ChunkManager* manager = calloc(1, sizeof(*manager) + initialCapacity*(sizeof(ClientChunk*)));
    if (!manager) {
        nob_log(3, "Failed to allocate chunk manager");
    }
    else {
        nob_log(3, "Allocated chunk manager chunks\n");
        // Count already to 0
        manager->capacity = initialCapacity;
    }
    return manager;
}

void FreeChunkManager(ChunkManager* manager)
{
    for (int i = 0; i < manager->capacity; i++)
    {
        if (manager->chunks[i])
            FreeClientChunk(manager->chunks[i]);
    }
    free(manager);
    nob_log(3, "Freed chunk manager\n");
}

// Chunk operations
// Allocate memory for ClientChunk
ClientChunk* CreateClientChunk(int x, int z)
{
    ClientChunk* chunk = calloc(1, sizeof(ClientChunk));
    if (!chunk) return NULL;

    chunk->x = x;
    chunk->z = z;

    nob_log(3, "Created chunk at (%d, %d)\n", x, z);

    return chunk;
}

void FreeClientChunk(ClientChunk* chunk)
{

    if (!chunk)
    {
        nob_log(3, "Unable to free chunk: Chunk is NULL\n");
        return;
    }
    
    int x = chunk->x;
    int z = chunk->z;

    nob_log(3, "Freeing chunk at (%d, %d)\n", x, z);
    
    nob_log(2, "FreeClientChunk : Mesh Need to be unloaded later\n");
    UnloadMesh(chunk->mesh);
    
    for(int y = 0; y < CHUNK_SIZE; y++)
    {
        free(chunk->data.verticals[y]);
    }

    free(chunk);
    nob_log(3, "Freed chunk at (%d, %d)\n", x, z);
    
}


ClientChunk* GetChunk(ChunkManager* manager, int x, int z)
{
    nob_log(3, "Retrieving chunk at (%d, %d)\n", x, z);
    for (int i = 0; i < manager->capacity; i++)
    {
        if (manager->chunks[i]          &&
            manager->chunks[i]->loaded  &&
            manager->chunks[i]->x == x  &&
            manager->chunks[i]->z == z)
        {
            
            nob_log(3, "Chunk retrieved at (%d, %d)\n", x, z);
            return manager->chunks[i];
        }
    }
    return NULL;
}

void AddChunk(ChunkManager* manager, ClientChunk* chunk)
{
    if (!chunk)
    {
        nob_log(3, "Unable to add chunk: Chunk is NULL");
        return;
    }
    if (manager->count >= manager->capacity) 
    {
        nob_log(3, "Unable to add chunk: Chunk manager is full");
        return;
    }

    for (int i = 0; i < manager->capacity; i++)
    {
        if (!manager->chunks[i])
        {
            manager->chunks[i] = chunk;
            manager->count++;
            return;
        }
    }    
    nob_log(3, "Chunk added at (%d, %d)\n", chunk->x, chunk->z);
    manager->count++;
}

void RemoveChunk(ChunkManager* manager, int index)
{
    ClientChunk* chunk = manager->chunks[index];
    int x = chunk->x;
    int z = chunk->z;
    nob_log(3, "Removing chunk at (%d, %d)\n", chunk->x, chunk->z);
 
    FreeClientChunk(manager->chunks[index]);
    manager->chunks[index] = NULL;
    manager->count--;
    nob_log(3, "Chunk at (%d, %d) successfully removed\n", x, z);
    return;
}

Vector3Int worldToChunkCoords(const Vector3* worldPos)
{
    return (Vector3Int) {worldPos->x / CHUNK_SIZE,
                         worldPos->y / CHUNK_SIZE,
                         worldPos->z / CHUNK_SIZE};
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
    printf("Unloaded Chunks at %d, %d\n", playerChunk.x, playerChunk.z);
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
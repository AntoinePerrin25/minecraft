#include "raylib.h"
#include "chunk_manager.h"
#define NOB_IMPLEMENTATION
#include "../nob.h"
#include <math.h>

// Chunk manager initialization and cleanup
ChunkManager InitChunkManager(int initialCapacity)
{
    ChunkManager manager;
    manager.count = 0;
    manager.capacity = initialCapacity;
    manager.chunks = calloc(initialCapacity, sizeof(ClientChunk*));
    if (!manager.chunks)
    {
        nob_log(3, "Failed to allocate chunk manager");
    }
    else
    {
        nob_log(3, "Allocated chunk manager chunks\n");
        return manager;
    }
    return manager;
}

void FreeChunkManager(ChunkManager manager)
{
    for (int i = 0; i < manager.count; i++)
    {
        if (manager.chunks[i])
            FreeClientChunk(manager.chunks[i]);
    }
    free(manager.chunks);
    nob_log(3, "Freed chunk manager\n");
}

// Chunk operations
// Allocate memory for ClientChunk
ClientChunk* CreateClientChunk(int x, int z)
{
    ClientChunk* chunk = malloc(sizeof(ClientChunk));
    if (!chunk) return NULL;

    chunk->data = malloc(sizeof(ChunkData));
    if (!chunk->data)
    {
        free(chunk);
        chunk = NULL;
        return NULL;
    }

    chunk->mesh = malloc(sizeof(Mesh));
    if (!chunk->mesh)
    {
        free(chunk->data);
        free(chunk);
        chunk = NULL;
        return NULL;
    }
    
    chunk->x = x;
    chunk->z = z;
    chunk->loaded = 0;
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
    
    if (chunk->mesh)
    {
        UnloadMesh(*chunk->mesh);
        free(chunk->mesh);
    }
    if (chunk->data)
    {
        free(chunk->data);
    }
    free(chunk);
    chunk = NULL;
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

void RemoveChunk(ChunkManager* manager, int x, int z)
{
    nob_log(3, "Removing chunk at (%d, %d)\n", x, z);
    for (int i = 0; i < manager->capacity; i++)
    {
        if (manager->chunks[i] &&
            manager->chunks[i]->x == x &&
            manager->chunks[i]->z == z)
        {
            FreeClientChunk(manager->chunks[i]);
            manager->chunks[i] = NULL;
            manager->count--;
            nob_log(3, "Chunk at (%d, %d) successfully removed\n", x, z);
            return;
        }
    }
}

Vector2 worldToChunkCoords(Vector3 worldPos)
{
    return (Vector2) {floor(worldPos.x / CHUNK_SIZE), floor(worldPos.z / CHUNK_SIZE)};
}

void unloadDistantChunks(ChunkManager* manager, Vector3 playerPos)
{
    int playerChunkX = (int)floor(playerPos.x / CHUNK_SIZE);
    int playerChunkZ = (int)floor(playerPos.z / CHUNK_SIZE);
    
    for (int i = 0; i < manager->count; i++)
    {
        if (!manager->chunks[i] || !manager->chunks[i]->loaded) continue;
        
        int dx = abs(manager->chunks[i]->x - playerChunkX);
        int dz = abs(manager->chunks[i]->z - playerChunkZ);
        
        if (dx > RENDER_DISTANCE || dz > RENDER_DISTANCE)
        {
            // Free the resources of the chunk
            RemoveChunk(manager, manager->chunks[i]->x, manager->chunks[i]->z);
        }
    }
}

// Chunk vertical operations
ChunkVertical* CreateChunkVertical(unsigned int y)
{
    ChunkVertical* vertical = malloc(sizeof(ChunkVertical));
    if (!vertical) return NULL;

    vertical->verticaly = y;
    vertical->isOnlyBlockType = 1;
    vertical->blockType = BLOCK_AIR;

    return vertical;
}

void FreeChunkVertical(ChunkVertical* vertical)
{
    if (vertical) {
        free(vertical);
    }
    
    vertical = NULL;
}

void reloadMesh(ClientChunk* chunk, Mesh mesh)
{
    if (!chunk) return;
    if (!chunk->data) return;
    if (!chunk->mesh) return;

    // Unload the mesh
    UnloadMesh(*chunk->mesh);

    // Allocate memory for the mesh
    *chunk->mesh = mesh;
}

#ifndef WORLD_MANAGER_H
#define WORLD_MANAGER_H

#include "chunk_manager.h"

#define WORLD_DIR "./build/world"

// Structure du chunk en cache
typedef struct {
    int x, z;                // Position in the world
    FullChunk chunk;         // Changed from ChunkData to FullChunk
    bool modified;           // Has been modified since last save
} CachedChunk;

// Gestionnaire de monde
typedef struct {
    int seed;                // World seed for terrain generation
    CachedChunk* chunks;     // Array of cached chunks
    int chunkCount;          // Number of chunks currently cached
    int chunkCapacity;       // Capacity of chunks array
} WorldManager;

// Gets the chunk filename
void getChunkFilename(char* buffer, int x, int z);

// Create world manager
WorldManager* worldManager_create(int seed);

// Get or generate chunk
FullChunk* worldManager_getChunk(WorldManager* wm, int x, int z); // Changed return type from ChunkData* to FullChunk*

// Save chunk to disk
void worldManager_saveChunk(WorldManager* wm, int x, int z);

// Save all modified chunks
void worldManager_saveAll(WorldManager* wm);

// Set block in the world
bool worldManager_setBlock(WorldManager* wm, int x, int y, int z, BlockData block);

// Free resources
void worldManager_destroy(WorldManager* wm);

#endif // WORLD_MANAGER_H
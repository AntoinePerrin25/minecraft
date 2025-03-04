#ifndef WORLD_MANAGER_H
#define WORLD_MANAGER_H

#include <stdbool.h>

#include "chunk_manager.h"

#define WORLD_DIR "world"

// Structure pour stocker un chunk en mémoire avec son état
typedef struct {
    ChunkData data;
    int x;
    int z;
    bool modified;
} CachedChunk;

// Structure pour le gestionnaire de monde
typedef struct {
    int seed;
    CachedChunk* chunks;
    int chunkCount;
    int chunkCapacity;
} WorldManager;

// Function declarations
WorldManager* worldManager_create(int seed);
void worldManager_destroy(WorldManager* wm);
ChunkData* worldManager_getChunk(WorldManager* wm, int x, int z);
void worldManager_saveChunk(WorldManager* wm, int x, int z);
bool worldManager_setBlock(WorldManager* wm, int x, int y, int z, BlockData block);
void worldManager_saveAll(WorldManager* wm);
CachedChunk* findChunk(WorldManager* wm, int x, int z);
void getChunkFilename(char* buffer, int x, int z);

#endif // WORLD_MANAGER_H
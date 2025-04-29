#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


#define nob_log(level, fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

#include "world_manager.h"
#include "world.h"
#include "chunk_manager.h"

// Initialise le gestionnaire de monde
WorldManager* worldManager_create(int seed) {
    WorldManager* wm = malloc(sizeof(WorldManager));
    if (!wm) {
        nob_log(NOB_ERROR, "Failed to allocate world manager");
        return NULL;
    }
    
    wm->seed = seed;
    wm->chunks = NULL;
    wm->chunkCount = 0;
    wm->chunkCapacity = 0;
    
    // Créer le dossier world s'il n'existe pas
    #ifdef _WIN32
    mkdir(WORLD_DIR);
    #else
    mkdir(WORLD_DIR, 0777);
    #endif
    
    nob_log(NOB_LEVEL_DEF, "Created world manager with seed %d", seed);
    return wm;
}

// Génère le nom du fichier pour un chunk
void getChunkFilename(char* buffer, int x, int z) {
    sprintf(buffer, WORLD_DIR "/chunk_%d_%d.dat", x, z);
}

// Trouve un chunk dans le cache
CachedChunk* findChunk(WorldManager* wm, int x, int z) {
    for (int i = 0; i < wm->chunkCount; i++) {
        if (wm->chunks[i].x == x && wm->chunks[i].z == z) {
            return &wm->chunks[i];
        }
    }
    return NULL;
}

// Charge un chunk depuis le disque ou le génère
ChunkData* worldManager_getChunk(WorldManager* wm, int x, int z) {
    // Vérifier si le chunk est en cache
    CachedChunk* cached = findChunk(wm, x, z);
    if (cached) {
        nob_log(NOB_LEVEL_DEF, "Chunk at (%d, %d) found in cache", x, z);
        return &cached->data;
    }
    
    nob_log(NOB_LEVEL_DEF, "Generating chunk at (%d, %d)", x, z);
    
    // Essayer de charger depuis le disque
    char filename[256];
    getChunkFilename(filename, x, z);
    FILE* f = fopen(filename, "rb");
    
    // Allouer de l'espace pour le nouveau chunk
    if (wm->chunkCount == wm->chunkCapacity) {
        wm->chunkCapacity = wm->chunkCapacity ? wm->chunkCapacity * 2 : 16;
        wm->chunks = realloc(wm->chunks, wm->chunkCapacity * sizeof(CachedChunk));
        if (!wm->chunks) {
            nob_log(NOB_ERROR, "Failed to reallocate chunk array");
            return NULL;
        }
    }
    
    CachedChunk* newChunk = &wm->chunks[wm->chunkCount++];
    memset(newChunk, 0, sizeof(CachedChunk));
    newChunk->modified = false;
    newChunk->x = x;
    newChunk->z = z;
    
    if (f) {
        // Charger depuis le fichier
        nob_log(NOB_LEVEL_DEF, "Loading chunk from file: %s", filename);
        fread(&newChunk->data, sizeof(ChunkData), 1, f);
        fclose(f);
    } else {
        // Générer un nouveau chunk
        nob_log(NOB_LEVEL_DEF, "Generating new terrain for chunk (%d, %d)", x, z);
        FullChunk fullChunk = generateChunk(x, z, wm->seed);
        newChunk->data = CompressChunk(&fullChunk);
        newChunk->modified = true;  // Marquer comme modifié pour sauvegarder plus tard
    }
    
    return &newChunk->data;
}

// Sauvegarde un chunk sur le disque
void worldManager_saveChunk(WorldManager* wm, int x, int z) {
    CachedChunk* chunk = findChunk(wm, x, z);
    if (!chunk || !chunk->modified) return;
    
    char filename[256];
    getChunkFilename(filename, x, z);
    FILE* f = fopen(filename, "wb");
    if (f) {
        nob_log(NOB_LEVEL_DEF, "Saving chunk to file: %s", filename);
        fwrite(&chunk->data, sizeof(ChunkData), 1, f);
        fclose(f);
        chunk->modified = false;
    } else {
        nob_log(NOB_ERROR, "Failed to open file for writing: %s", filename);
    }
}

// Modifie un block dans un chunk
bool worldManager_setBlock(WorldManager* wm, int x, int y, int z, BlockData block) {
    // Convertir les coordonnées monde en coordonnées chunk
    int chunkX = floor((float)x / CHUNK_SIZE);
    int chunkZ = floor((float)z / CHUNK_SIZE);
    
    // Coordonnées relatives au chunk
    int localX = x - chunkX * CHUNK_SIZE;
    int localZ = z - chunkZ * CHUNK_SIZE;
    int localY = y % CHUNK_SIZE;
    int section = y / CHUNK_SIZE;
    
    if (localX < 0) localX += CHUNK_SIZE;
    if (localZ < 0) localZ += CHUNK_SIZE;
    
    // Vérifier les limites
    if (y < 0 || y >= WORLD_HEIGHT || section >= CHUNK_SIZE) {
        nob_log(NOB_ERROR, "Block position out of bounds: (%d, %d, %d)", x, y, z);
        return false;
    }
    
    ChunkData* chunkData = worldManager_getChunk(wm, chunkX, chunkZ);
    if (!chunkData) {
        nob_log(NOB_ERROR, "Failed to get chunk data for position (%d, %d, %d)", x, y, z);
        return false;
    }
    
    // Ne pas permettre la modification de la bedrock
    if (y == 0 && block.Type != BLOCK_BEDROCK) {
        nob_log(NOB_WARNING, "Cannot modify bedrock at position (%d, %d, %d)", x, y, z);
        return false;
    }
    
    // Check if we need to allocate a vertical section
    if (chunkData->verticals[section] == NULL) {
        // If we're setting to the same block type as the current uniform block, do nothing
        if (block.Type == chunkData->blockType[section]) {
            return true;
        }
        
        // Need to allocate and fill with the uniform block type
        nob_log(NOB_LEVEL_DEF, "Allocating new vertical section for chunk (%d, %d) at section %d", 
                chunkX, chunkZ, section);
        chunkData->verticals[section] = CreateChunkVertical();
        if (!chunkData->verticals[section]) {
            nob_log(NOB_ERROR, "Failed to allocate vertical chunk section");
            return false;
        }
        
        // Fill with the current block type
        BlockType currentType = chunkData->blockType[section];
        
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                for (int z = 0; z < CHUNK_SIZE; z++) {
                    BlockData fillBlock = {0};
                    fillBlock.Type = currentType;
                    fillBlock.solid = (currentType != BLOCK_AIR && currentType != BLOCK_WATER);
                    fillBlock.visible = (currentType != BLOCK_AIR);
                    
                    chunkData->verticals[section]->blocks[y][x][z] = fillBlock;
                }
            }
        }
        
        chunkData->blockType[section] = BLOCK_NONE; // No longer uniform
    }
    
    // Modifier le block
    chunkData->verticals[section]->blocks[localY][localX][localZ] = block;
    CachedChunk* cached = findChunk(wm, chunkX, chunkZ);
    if (cached) {
        cached->modified = true;
        nob_log(NOB_LEVEL_DEF, "Modified block at (%d, %d, %d) to type %d", x, y, z, block.Type);
    }
    
    return true;
}

// Sauvegarde tous les chunks modifiés
void worldManager_saveAll(WorldManager* wm) {
    nob_log(NOB_LEVEL_DEF, "Saving all modified chunks");
    for (int i = 0; i < wm->chunkCount; i++) {
        if (wm->chunks[i].modified) {
            worldManager_saveChunk(wm, wm->chunks[i].x, wm->chunks[i].z);
        }
    }
}

// Libère la mémoire
void worldManager_destroy(WorldManager* wm) {
    if (!wm) return;
    
    nob_log(NOB_LEVEL_DEF, "Destroying world manager");
    worldManager_saveAll(wm);
    
    // Free all chunk data
    for (int i = 0; i < wm->chunkCount; i++) {
        for (int j = 0; j < CHUNK_SIZE; j++) {
            free(wm->chunks[i].data.verticals[j]);
        }
    }
    
    free(wm->chunks);
    free(wm);
}

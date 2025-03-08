#ifndef WORLD_MANAGER_H
#define WORLD_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "world.h"

#define WORLD_DIR "world"

// Structure pour stocker un chunk en mémoire avec son état
typedef struct {
    ChunkData data;
    bool modified;
} CachedChunk;

// Structure pour le gestionnaire de monde
typedef struct {
    int seed;
    CachedChunk* chunks;
    int chunkCount;
    int chunkCapacity;
} WorldManager;

// Initialise le gestionnaire de monde
static WorldManager* worldManager_create(int seed) {
    WorldManager* wm = malloc(sizeof(WorldManager));
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
    
    return wm;
}

// Génère le nom du fichier pour un chunk
static void getChunkFilename(char* buffer, int x, int z) {
    sprintf(buffer, WORLD_DIR "/chunk_%d_%d.dat", x, z);
}

// Trouve un chunk dans le cache
static CachedChunk* findChunk(WorldManager* wm, int x, int z) {
    for (int i = 0; i < wm->chunkCount; i++) {
        if (wm->chunks[i].data.x == x && wm->chunks[i].data.z == z) {
            return &wm->chunks[i];
        }
    }
    return NULL;
}

// Charge un chunk depuis le disque ou le génère
static ChunkData* worldManager_getChunk(WorldManager* wm, int x, int z) {
    // Vérifier si le chunk est en cache
    CachedChunk* cached = findChunk(wm, x, z);
    if (cached) {
        return &cached->data;
    }
    
    // Essayer de charger depuis le disque
    char filename[256];
    getChunkFilename(filename, x, z);
    FILE* f = fopen(filename, "rb");
    
    // Allouer de l'espace pour le nouveau chunk
    if (wm->chunkCount == wm->chunkCapacity) {
        wm->chunkCapacity = wm->chunkCapacity ? wm->chunkCapacity * 2 : 16;
        wm->chunks = realloc(wm->chunks, wm->chunkCapacity * sizeof(CachedChunk));
    }
    
    CachedChunk* newChunk = &wm->chunks[wm->chunkCount++];
    newChunk->modified = false;
    
    if (f) {
        // Charger depuis le fichier
        fread(&newChunk->data, sizeof(ChunkData), 1, f);
        fclose(f);
    } else {
        // Générer un nouveau chunk
        generateChunk(&newChunk->data, x, z, wm->seed);
        newChunk->modified = true;  // Marquer comme modifié pour sauvegarder plus tard
    }
    
    return &newChunk->data;
}

// Sauvegarde un chunk sur le disque
static void worldManager_saveChunk(WorldManager* wm, int x, int z) {
    CachedChunk* chunk = findChunk(wm, x, z);
    if (!chunk || !chunk->modified) return;
    
    char filename[256];
    getChunkFilename(filename, x, z);
    FILE* f = fopen(filename, "wb");
    if (f) {
        fwrite(&chunk->data, sizeof(ChunkData), 1, f);
        fclose(f);
        chunk->modified = false;
    }
}

// Modifie un block dans un chunk
static bool worldManager_setBlock(WorldManager* wm, int x, int y, int z, BlockType type) {
    // Convertir les coordonnées monde en coordonnées chunk
    int chunkX = x / CHUNK_SIZE;
    int chunkZ = z / CHUNK_SIZE;
    
    // Coordonnées relatives au chunk
    int localX = x % CHUNK_SIZE;
    int localZ = z % CHUNK_SIZE;
    
    if (localX < 0) {
        localX += CHUNK_SIZE;
        chunkX--;
    }
    if (localZ < 0) {
        localZ += CHUNK_SIZE;
        chunkZ--;
    }
    
    // Vérifier les limites
    if (y < 0 || y >= WORLD_HEIGHT) return false;
    
    ChunkData* chunk = worldManager_getChunk(wm, chunkX, chunkZ);
    if (!chunk) return false;
    
    // Ne pas permettre la modification de la bedrock
    if (y == 0) return false;
    
    // Modifier le block
    chunk->blocks[localX][y][localZ].Type = type;
    CachedChunk* cached = findChunk(wm, chunkX, chunkZ);
    if (cached) cached->modified = true;
    
    return true;
}

// Sauvegarde tous les chunks modifiés
static void worldManager_saveAll(WorldManager* wm) {
    for (int i = 0; i < wm->chunkCount; i++) {
        if (wm->chunks[i].modified) {
            worldManager_saveChunk(wm, wm->chunks[i].data.x, wm->chunks[i].data.z);
        }
    }
}

// Libère la mémoire
static void worldManager_destroy(WorldManager* wm) {
    if (!wm) return;
    worldManager_saveAll(wm);
    free(wm->chunks);
    free(wm);
}

#endif // WORLD_MANAGER_H
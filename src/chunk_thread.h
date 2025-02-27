#ifndef CHUNK_THREAD_H
#define CHUNK_THREAD_H

#ifdef _WIN32
#define WINDOWS_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#endif

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

// Required raylib includes
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include "network.h"
#include "chunk_mesh.h"
#include "rnet.h"    // For rnetPeer type

#define MAX_CHUNK_QUEUE 64
#define MAX_MESH_WORKERS 4
#define MAX_CACHED_CHUNKS 32  // Reduced from 100 to save memory

typedef struct {
    int x, z;
    uint64_t lastAccess;
} ChunkKey;

typedef struct {
    ChunkData data;
    ChunkMesh mesh;
    bool loaded;
    uint64_t lastAccess;
    bool needsMeshUpdate;
    pthread_mutex_t mutex;
} ThreadedChunk;

typedef struct {
    ChunkKey key;
    bool isRequest;  // true = chunk request, false = mesh update
} ChunkQueueItem;

typedef struct ChunkThreadManager {
    ThreadedChunk chunks[MAX_CACHED_CHUNKS];
    int chunkCount;
    
    // Queue pour les requêtes de chunk et génération de mesh
    ChunkQueueItem queue[MAX_CHUNK_QUEUE];
    int queueHead;
    int queueTail;
    
    // Synchronisation
    pthread_t meshWorkers[MAX_MESH_WORKERS];
    pthread_t loadThread;
    pthread_mutex_t queueMutex;
    pthread_cond_t queueCond;
    bool running;
    
    // Pointeur vers le client réseau 
    rnetPeer* client;
} ChunkThreadManager;

static uint64_t getCurrentTimestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static bool isQueueEmpty(ChunkThreadManager* tm) {
    return tm->queueHead == tm->queueTail;
}

static bool isQueueFull(ChunkThreadManager* tm) {
    return ((tm->queueTail + 1) % MAX_CHUNK_QUEUE) == tm->queueHead;
}

static void enqueueChunk(ChunkThreadManager* tm, int x, int z, bool isRequest) {
    pthread_mutex_lock(&tm->queueMutex);
    
    if (!isQueueFull(tm)) {
        tm->queue[tm->queueTail].key.x = x;
        tm->queue[tm->queueTail].key.z = z;
        tm->queue[tm->queueTail].isRequest = isRequest;
        tm->queueTail = (tm->queueTail + 1) % MAX_CHUNK_QUEUE;
        pthread_cond_signal(&tm->queueCond);
    }
    
    pthread_mutex_unlock(&tm->queueMutex);
}

static bool dequeueChunk(ChunkThreadManager* tm, ChunkQueueItem* item) {
    pthread_mutex_lock(&tm->queueMutex);
    
    while (isQueueEmpty(tm) && tm->running) {
        pthread_cond_wait(&tm->queueCond, &tm->queueMutex);
    }
    
    bool success = false;
    if (!isQueueEmpty(tm)) {
        *item = tm->queue[tm->queueHead];
        tm->queueHead = (tm->queueHead + 1) % MAX_CHUNK_QUEUE;
        success = true;
    }
    
    pthread_mutex_unlock(&tm->queueMutex);
    return success;
}

static ThreadedChunk* findChunk(ChunkThreadManager* tm, int x, int z) {
    for (int i = 0; i < tm->chunkCount; i++) {
        if (tm->chunks[i].loaded) {
            pthread_mutex_lock(&tm->chunks[i].mutex);
            if (tm->chunks[i].data.x == x && tm->chunks[i].data.z == z) {
                tm->chunks[i].lastAccess = getCurrentTimestamp();
                pthread_mutex_unlock(&tm->chunks[i].mutex);
                return &tm->chunks[i];
            }
            pthread_mutex_unlock(&tm->chunks[i].mutex);
        }
    }
    return NULL;
}

static void evictLRUChunk(ChunkThreadManager* tm) {
    if (tm->chunkCount == 0) return;
    
    int lruIdx = 0;
    uint64_t oldestAccess = UINT64_MAX;
    
    // Trouver le chunk le moins récemment utilisé
    for (int i = 0; i < tm->chunkCount; i++) {
        pthread_mutex_lock(&tm->chunks[i].mutex);
        if (tm->chunks[i].loaded && tm->chunks[i].lastAccess < oldestAccess) {
            oldestAccess = tm->chunks[i].lastAccess;
            lruIdx = i;
        }
        pthread_mutex_unlock(&tm->chunks[i].mutex);
    }
    
    // Libérer le chunk
    pthread_mutex_lock(&tm->chunks[lruIdx].mutex);
    if (tm->chunks[lruIdx].loaded) {
        freeChunkMesh(&tm->chunks[lruIdx].mesh);
        tm->chunks[lruIdx].loaded = false;
    }
    pthread_mutex_unlock(&tm->chunks[lruIdx].mutex);
}

static void* meshWorkerThread(void* arg) {
    ChunkThreadManager* tm = (ChunkThreadManager*)arg;
    ChunkQueueItem item;
    
    while (tm->running) {
        if (dequeueChunk(tm, &item) && !item.isRequest) {
            printf("Mesh worker processing chunk (%d, %d)\n", item.key.x, item.key.z);
            ThreadedChunk* chunk = findChunk(tm, item.key.x, item.key.z);
            if (chunk) {
                pthread_mutex_lock(&chunk->mutex);
                if (chunk->needsMeshUpdate) {
                    printf("Updating mesh for chunk (%d, %d)\n", item.key.x, item.key.z);
                    updateChunkMesh(&chunk->mesh, &chunk->data);
                    chunk->needsMeshUpdate = false;
                    printf("Mesh update complete for chunk (%d, %d)\n", item.key.x, item.key.z);
                }
                pthread_mutex_unlock(&chunk->mutex);
            } else {
                printf("WARNING: Could not find chunk (%d, %d) for mesh update\n", 
                       item.key.x, item.key.z);
            }
        }
    }
    
    return NULL;
}

static void* chunkLoadThread(void* arg) {
    ChunkThreadManager* tm = (ChunkThreadManager*)arg;
    ChunkQueueItem item;
    
    while (tm->running) {
        if (dequeueChunk(tm, &item) && item.isRequest) {
            // Envoyer la requête au serveur
            Packet packet = {
                .type = PACKET_CHUNK_REQUEST,
                .chunkRequest = {
                    .chunkX = item.key.x,
                    .chunkZ = item.key.z
                }
            };
            rnetSend(tm->client, &packet, sizeof(Packet), RNET_RELIABLE);
        }
    }
    
    return NULL;
}

static void initChunkThreadManager(ChunkThreadManager* tm, rnetPeer* client) {
    tm->chunkCount = 0;
    tm->queueHead = 0;
    tm->queueTail = 0;
    tm->running = true;
    tm->client = client;
    
    pthread_mutex_init(&tm->queueMutex, NULL);
    pthread_cond_init(&tm->queueCond, NULL);
    
    // Initialiser les chunks
    for (int i = 0; i < MAX_CACHED_CHUNKS; i++) {
        pthread_mutex_init(&tm->chunks[i].mutex, NULL);
        tm->chunks[i].loaded = false;
        tm->chunks[i].needsMeshUpdate = false;
    }
    
    // Démarrer les threads
    pthread_create(&tm->loadThread, NULL, chunkLoadThread, tm);
    for (int i = 0; i < MAX_MESH_WORKERS; i++) {
        pthread_create(&tm->meshWorkers[i], NULL, meshWorkerThread, tm);
    }
}

static void destroyChunkThreadManager(ChunkThreadManager* tm) {
    tm->running = false;
    
    // Réveiller tous les threads
    pthread_mutex_lock(&tm->queueMutex);
    pthread_cond_broadcast(&tm->queueCond);
    pthread_mutex_unlock(&tm->queueMutex);
    
    // Attendre la fin des threads
    pthread_join(tm->loadThread, NULL);
    for (int i = 0; i < MAX_MESH_WORKERS; i++) {
        pthread_join(tm->meshWorkers[i], NULL);
    }
    
    // Nettoyer les ressources
    pthread_mutex_destroy(&tm->queueMutex);
    pthread_cond_destroy(&tm->queueCond);
    
    for (int i = 0; i < MAX_CACHED_CHUNKS; i++) {
        if (tm->chunks[i].loaded) {
            freeChunkMesh(&tm->chunks[i].mesh);
        }
        pthread_mutex_destroy(&tm->chunks[i].mutex);
    }
}

static void handleChunkUpdate(ChunkThreadManager* tm, ChunkData* chunk) {
    printf("Received chunk update for chunk (%d, %d)\n", chunk->x, chunk->z);
    
    // Trouver un slot libre ou évincer le chunk le moins récemment utilisé
    ThreadedChunk* target = findChunk(tm, chunk->x, chunk->z);
    
    if (!target) {
        if (tm->chunkCount >= MAX_CACHED_CHUNKS) {
            printf("Evicting LRU chunk to make space\n");
            evictLRUChunk(tm);
        }
        
        // Trouver un slot libre
        for (int i = 0; i < MAX_CACHED_CHUNKS; i++) {
            if (!tm->chunks[i].loaded) {
                target = &tm->chunks[i];
                if (i >= tm->chunkCount) {
                    tm->chunkCount = i + 1;
                }
                printf("Created new chunk slot at index %d\n", i);
                break;
            }
        }
    }
    
    if (target) {
        pthread_mutex_lock(&target->mutex);
        target->data = *chunk;
        target->loaded = true;
        target->lastAccess = getCurrentTimestamp();
        target->needsMeshUpdate = true;
        printf("Updated chunk data and queued mesh update\n");
        pthread_mutex_unlock(&target->mutex);
        
        // Mettre en file d'attente la mise à jour du mesh
        enqueueChunk(tm, chunk->x, chunk->z, false);
    } else {
        printf("ERROR: Failed to find space for new chunk!\n");
    }
}

static void requestChunk(ChunkThreadManager* tm, int x, int z) {
    if (!findChunk(tm, x, z)) {
        enqueueChunk(tm, x, z, true);
    }
}

#endif // CHUNK_THREAD_H
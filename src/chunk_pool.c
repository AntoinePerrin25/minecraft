#include "chunk_pool.h"

#include "config.h"
#include "mesh.h"
#include "world.h"

#include <stdlib.h>

void ChunkPoolInit(ChunkPool *pool)
{
    pool->maxChunks = CHUNK_POOL_SIZE;
    pool->chunks = calloc((size_t)pool->maxChunks, sizeof(Chunk));
    pool->lastPlayerChunk = (Vector2Int){ -9999, -9999 };

    for (int i = 0; i < pool->maxChunks; i++) {
        pool->chunks[i].active = 0;
        pool->chunks[i].loaded = 0;
        pool->chunks[i].render.hasMesh = 0;
        pool->chunks[i].render.meshReady = 0;
    }

    WorldInit(WORLD_SEED, pool->maxChunks);
}

void ChunkPoolFree(ChunkPool *pool)
{
    free(pool->chunks);
    pool->chunks = NULL;
    pool->maxChunks = 0;
}

void ChunkPoolUpdate(ChunkPool *pool, Vector2Int playerChunk)
{
    if (playerChunk.x == pool->lastPlayerChunk.x &&
        playerChunk.z == pool->lastPlayerChunk.z) {
        return;
    }

    for (int i = 0; i < pool->maxChunks; i++) {
        if (!pool->chunks[i].active) continue;

        int dx = pool->chunks[i].x - playerChunk.x;
        int dz = pool->chunks[i].z - playerChunk.z;

        if (abs(dx) > CHUNK_LOAD_DISTANCE || abs(dz) > CHUNK_LOAD_DISTANCE) {
            UnloadChunkMesh(i);
            pool->chunks[i].active = 0;
            pool->chunks[i].loaded = 0;
        }
    }

    for (int x = -CHUNK_LOAD_DISTANCE; x <= CHUNK_LOAD_DISTANCE; x++) {
        for (int z = -CHUNK_LOAD_DISTANCE; z <= CHUNK_LOAD_DISTANCE; z++) {
            int chunkX = playerChunk.x + x;
            int chunkZ = playerChunk.z + z;

            int found = 0;
            for (int i = 0; i < pool->maxChunks; i++) {
                if (pool->chunks[i].active &&
                    pool->chunks[i].x == chunkX &&
                    pool->chunks[i].z == chunkZ) {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                for (int i = 0; i < pool->maxChunks; i++) {
                    if (!pool->chunks[i].active) {
                        UnloadChunkMesh(i);
                        generateChunk(&pool->chunks[i], chunkX, chunkZ);
                        pool->chunks[i].active = 1;
                        pool->chunks[i].loaded = 1;
                        ScheduleChunkRemesh(i, 0);
                        break;
                    }
                }
            }
        }
    }

    pool->lastPlayerChunk = playerChunk;
}

#ifndef MC_CHUNK_POOL_H
#define MC_CHUNK_POOL_H

#include "chunk.h"
#include "types.h"

typedef struct ChunkPool {
    Chunk *chunks;
    int maxChunks;
    Vector2Int lastPlayerChunk;
} ChunkPool;

void ChunkPoolInit(ChunkPool *pool);
void ChunkPoolFree(ChunkPool *pool);
void ChunkPoolUpdate(ChunkPool *pool, Vector2Int playerChunk);

#endif

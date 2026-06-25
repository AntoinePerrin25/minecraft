#ifndef MC_WORLD_H
#define MC_WORLD_H

#include "block.h"
#include "chunk.h"
#include "types.h"
#include "raylib.h"

void WorldInit(uint64_t seed, int chunkCount);
void generateChunk(Chunk *chunk, int chunkX, int chunkZ);
BlockData getBlockAt(Chunk *chunks, int worldX, int worldY, int worldZ);
BlockInWorld worldToBlockCoords(Vector3 worldPos);
int isBlockExposed(Chunk *chunks, int x, int y, int z);

#endif

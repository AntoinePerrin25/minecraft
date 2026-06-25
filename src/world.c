#include "world.h"

#include "config.h"
#include "perlin.h"

#include <math.h>
#include <stdlib.h>

static int g_totalChunks = 0;
static PerlinNoise *g_terrain_noise = NULL;
static PerlinNoise *g_detail_noise = NULL;

void WorldInit(uint64_t seed, int chunkCount)
{
    g_totalChunks = chunkCount;
    if (g_terrain_noise) perlin_free(g_terrain_noise);
    if (g_detail_noise) perlin_free(g_detail_noise);
    g_terrain_noise = perlin_init(seed);
    g_detail_noise = perlin_init(seed ^ 0xdeadbeef);
}

void generateChunk(Chunk *chunk, int chunkX, int chunkZ)
{
    chunk->x = chunkX;
    chunk->z = chunkZ;

    if (!g_terrain_noise) return;

    int worldXBase = chunkX << 4;
    int worldZBase = chunkZ << 4;

    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < CHUNK_SIZE; lz++) {
            int wx = worldXBase + lx;
            int wz = worldZBase + lz;

            float terrain = perlin_octave(g_terrain_noise, wx * 0.01f, 0, wz * 0.01f, 3, 0.5f);
            float detail = perlin_noise3d(g_detail_noise, wx * 0.05f, 0, wz * 0.05f);
            float height_factor = terrain * 0.8f + detail * 0.2f;

            int max_height = 40 + (int)((height_factor + 1.0f) * 20.0f);
            max_height = max_height < 5 ? 5 : (max_height > 100 ? 100 : max_height);

            for (int y = 0; y < WORLD_HEIGHT; y++) {
                BlockData block;

                if (y < 4) {
                    block = createBlock(BLOCK_BEDROCK);
                } else if (y < max_height - 3) {
                    block = createBlock(BLOCK_STONE);
                } else if (y < max_height - 1) {
                    block = createBlock(BLOCK_DIRT);
                } else if (y == max_height - 1) {
                    block = createBlock(BLOCK_GRASS);
                } else {
                    block = createBlock(BLOCK_AIR);
                }

                chunk->data.blocks[lx][y][lz] = block;
            }
        }
    }
}

BlockInWorld worldToBlockCoords(Vector3 worldPos)
{
    BlockInWorld biw;
    biw.blockCoord.x = (int)floorf(worldPos.x) & 15;
    biw.blockCoord.y = (int)floorf(worldPos.y);
    biw.blockCoord.z = (int)floorf(worldPos.z) & 15;
    biw.chunkCoord.x = (int)floorf(worldPos.x) >> 4;
    biw.chunkCoord.z = (int)floorf(worldPos.z) >> 4;
    return biw;
}

BlockData getBlockAt(Chunk *chunks, int worldX, int worldY, int worldZ)
{
    if (worldY < 0 || worldY >= WORLD_HEIGHT) {
        return createBlock(BLOCK_AIR);
    }

    int chunkX = (worldX >= 0) ? (worldX >> 4) : ((worldX + 1) / 16 - 1);
    int chunkZ = (worldZ >= 0) ? (worldZ >> 4) : ((worldZ + 1) / 16 - 1);

    int localX = worldX - (chunkX * 16);
    int localZ = worldZ - (chunkZ * 16);

    if (localX < 0 || localX >= CHUNK_SIZE || localZ < 0 || localZ >= CHUNK_SIZE) {
        return createBlock(BLOCK_AIR);
    }

    for (int i = 0; i < g_totalChunks; i++) {
        if (chunks[i].active && chunks[i].x == chunkX && chunks[i].z == chunkZ) {
            return chunks[i].data.blocks[localX][worldY][localZ];
        }
    }

    return createBlock(BLOCK_AIR);
}

int isBlockExposed(Chunk *chunks, int x, int y, int z)
{
    int offsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };

    for (int i = 0; i < 6; i++) {
        int nx = x + offsets[i][0];
        int ny = y + offsets[i][1];
        int nz = z + offsets[i][2];
        BlockData neighbor = getBlockAt(chunks, nx, ny, nz);
        if (neighbor.Type == BLOCK_AIR || neighbor.Type == BLOCK_NONE) {
            return 1;
        }
    }

    return 0;
}

#ifndef WORLD_H
#define WORLD_H

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "chunk_manager.h"

#define CHUNK_SIZE 16
#define WORLD_HEIGHT 256
#define TERRAIN_HEIGHT 64  // Hauteur moyenne du terrain
#define TERRAIN_SCALE 50.0f  // Échelle du bruit de Perlin

// Perlin noise functions
static inline float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static inline float lerp(float t, float a, float b) {
    return a + t * (b - a);
}

static inline float grad(int hash, float x, float y) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}
static const int base_p[256] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
    247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
    74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
    65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
    52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
    119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
    218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,
    184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

static inline float noise2d(float x, float y, int seed) {
    // Initialiser le tableau de permutations avec la seed
    int p[512];
    
    // Mélanger le tableau en fonction de la seed
    srand(seed);
    for (int i = 0; i < 256; i++) {
        int j = rand() % 256;
        int temp = base_p[i];
        p[i] = base_p[j];
        p[j] = temp;
    }
    
    // Dupliquer le tableau pour éviter les calculs de modulo
    for (int i = 0; i < 256; i++) {
        p[256 + i] = p[i];
    }
    
    int xi = (int)floor(x) & 255;
    int yi = (int)floor(y) & 255;
    float xf = x - floor(x);
    float yf = y - floor(y);
    
    float u = fade(xf);
    float v = fade(yf);
    
    int aa = p[(p[xi] + yi) & 255];
    int ab = p[(p[xi] + yi + 1) & 255];
    int ba = p[(p[xi + 1] + yi) & 255];
    int bb = p[(p[xi + 1] + yi + 1) & 255];
    
    float x1 = lerp(u, grad(aa, xf, yf), grad(ba, xf - 1, yf));
    float x2 = lerp(u, grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1));
    
    return (lerp(v, x1, x2) + 1) / 2;
}

// Fonction pour générer la hauteur du terrain à une position donnée
static inline int getTerrainHeight(int x, int z, int seed) {
    float nx = x / TERRAIN_SCALE;
    float nz = z / TERRAIN_SCALE;
    
    float height = noise2d(nx, nz, seed);
    return (int)(height * TERRAIN_HEIGHT + TERRAIN_HEIGHT);
}

// Fonction pour générer un chunk
static inline FullChunk generateChunk(int chunkX, int chunkZ, int seed) {
    FullChunk chunk = {0};
    srand(seed + chunkX * 31 + chunkZ * 17);  // Use seed and chunk position to generate unique terrain
    
    // Generate terrain for each column
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            int worldX = chunkX * CHUNK_SIZE + x;
            int worldZ = chunkZ * CHUNK_SIZE + z;
            int surfaceHeight = getTerrainHeight(worldX, worldZ, seed);
            
            // Generate blocks for this column
            for (int y = 0; y < CHUNK_SIZE2; y++) {
                BlockData block = {0};
                
                // Bedrock layers (0-2 with random distribution)
                if (y == 0 || (y <= 2 && rand() % (y + 1) == 0)) {
                    block.Type = BLOCK_BEDROCK;
                    block.solid = 1;
                    block.visible = 1;
                }
                // Underground (stone)
                else if (y < surfaceHeight - 3) {
                    block.Type = BLOCK_STONE;
                    block.solid = 1;
                    block.visible = 1;
                    
                    // Generate ores
                    int r = rand() % 100;
                    
                    // Coal ore (5% chance between layers 5-50)
                    if (y >= 5 && y <= 50 && r < 5) {
                        // Coal ore would be represented by a different block type
                        // For now, we'll keep it as BLOCK_STONE since we don't have a coal type
                    }
                    
                    // Iron ore (3% chance between layers 5-40)
                    if (y >= 5 && y <= 40 && r < 3) {
                        // Iron ore representation
                    }
                    
                    // Gold ore (2% chance between layers 5-30)
                    if (y >= 5 && y <= 30 && r < 2) {
                        // Gold ore representation
                    }
                    
                    // Diamond ore (1% chance between layers 5-15)
                    if (y >= 5 && y <= 15 && r < 1) {
                        // Diamond ore representation
                    }
                }
                // Dirt layers (few blocks below surface)
                else if (y < surfaceHeight) {
                    block.Type = BLOCK_DIRT;
                    block.solid = 1;
                    block.visible = 1;
                }
                // Surface layer
                else if (y == surfaceHeight) {
                    block.Type = BLOCK_GRASS;
                    block.solid = 1;
                    block.visible = 1;
                    
                    // Generate tree (3% chance on grass blocks with enough space)
                    if (rand() % 100 < 3 && surfaceHeight + 6 < CHUNK_SIZE2) {
                        // Tree trunk (4 blocks high)
                        for (int treeY = 1; treeY <= 4; treeY++) {
                            if (y + treeY < CHUNK_SIZE2) {
                                chunk.blocks[y + treeY][x][z].Type = BLOCK_WOOD;
                                chunk.blocks[y + treeY][x][z].solid = 1;
                                chunk.blocks[y + treeY][x][z].visible = 1;
                            }
                        }
                        
                        // Tree leaves (simple 3x3x3 cube around the top of the trunk)
                        for (int ly = 3; ly <= 5; ly++) {
                            for (int lx = -1; lx <= 1; lx++) {
                                for (int lz = -1; lz <= 1; lz++) {
                                    int leafX = x + lx;
                                    int leafY = y + ly;
                                    int leafZ = z + lz;
                                    
                                    // Check boundaries
                                    if (leafX >= 0 && leafX < CHUNK_SIZE && 
                                        leafY >= 0 && leafY < CHUNK_SIZE2 && 
                                        leafZ >= 0 && leafZ < CHUNK_SIZE) {
                                        // Could use a BLOCK_LEAVES type if available
                                        chunk.blocks[leafY][leafX][leafZ].Type = BLOCK_WOOD;
                                        chunk.blocks[leafY][leafX][leafZ].solid = 1;
                                        chunk.blocks[leafY][leafX][leafZ].visible = 1;
                                    }
                                }
                            }
                        }
                    }
                }
                // Water blocks (where surface is below water level)
                else if (y <= 64 && surfaceHeight < 64) {  // Assuming water level is at y=64
                    block.Type = BLOCK_WATER;
                    block.solid = 0;  // Water is not solid
                    block.visible = 1;
                }
                // Air above the surface
                else {
                    block.Type = BLOCK_AIR;
                    block.solid = 0;
                    block.visible = 0;
                }
                
                // Set light level (simple approach: higher = more light)
                block.lightLevel = (uint16_t)((float)y / CHUNK_SIZE2 * MAX_LIGHT_LEVEL);
                
                // Assign the block to the chunk
                chunk.blocks[y][x][z] = block;
            }
        }
    }
    
    // Add sand around water (shorelines)
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int y = 60; y <= 66; y++) {  // Around water level
                if (chunk.blocks[y][x][z].Type == BLOCK_GRASS) {
                    // Check neighboring blocks for water
                    bool nearWater = false;
                    for (int nx = -1; nx <= 1 && !nearWater; nx++) {
                        for (int nz = -1; nz <= 1 && !nearWater; nz++) {
                            int checkX = x + nx;
                            int checkZ = z + nz;
                            if (checkX >= 0 && checkX < CHUNK_SIZE && checkZ >= 0 && checkZ < CHUNK_SIZE) {
                                if (chunk.blocks[y][checkX][checkZ].Type == BLOCK_WATER) {
                                    nearWater = true;
                                }
                            }
                        }
                    }
                    
                    if (nearWater) {
                        chunk.blocks[y][x][z].Type = BLOCK_SAND;
                        // Also convert a few blocks below to sand
                        int depthSand = rand() % 3 + 1;
                        for (int sy = 1; sy <= depthSand && y-sy >= 0; sy++) {
                            if (chunk.blocks[y-sy][x][z].Type == BLOCK_DIRT) {
                                chunk.blocks[y-sy][x][z].Type = BLOCK_SAND;
                            }
                        }
                    }
                }
            }
        }
    }
    
    return chunk;
}

#endif // WORLD_H
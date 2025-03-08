#ifndef WORLD_H
#define WORLD_H

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "network.h"

#define CHUNK_SIZE 16
#define WORLD_HEIGHT 256
#define TERRAIN_HEIGHT 64  // Hauteur moyenne du terrain
#define TERRAIN_SCALE 50.0f  // Échelle du bruit de Perlin

// Perlin noise functions
static float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static float lerp(float t, float a, float b) {
    return a + t * (b - a);
}

static float grad(int hash, float x, float y) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

static float noise2d(float x, float y, int seed) {
    // Initialiser le tableau de permutations avec la seed
    int p[512];
    const int base_p[256] = {
        151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
        247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
        74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
        65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
        52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
        119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
        218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,
        184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
    };
    
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
static int getTerrainHeight(int x, int z, int seed) {
    float nx = x / TERRAIN_SCALE;
    float nz = z / TERRAIN_SCALE;
    
    float height = noise2d(nx, nz, seed);
    return (int)(height * TERRAIN_HEIGHT + TERRAIN_HEIGHT);
}

// Fonction pour générer un chunk
static void generateChunk(ChunkData* chunk, int chunkX, int chunkZ, int seed) {
    chunk->x = chunkX;
    chunk->z = chunkZ;
    
    int worldX = chunkX * CHUNK_SIZE;
    int worldZ = chunkZ * CHUNK_SIZE;
    
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            int height = getTerrainHeight(worldX + x, worldZ + z, seed);
            
            for (int y = 0; y < WORLD_HEIGHT; y++) {
                if (y == 0) {
                    chunk->blocks[x][y][z].Type = BLOCK_BEDROCK;
                } else if (y < height - 5) {
                    chunk->blocks[x][y][z].Type = BLOCK_STONE;
                } else if (y < height - 1) {
                    chunk->blocks[x][y][z].Type = BLOCK_DIRT;
                } else if (y == height - 1) {
                    chunk->blocks[x][y][z].Type = BLOCK_GRASS;
                } else {
                    chunk->blocks[x][y][z].Type = BLOCK_AIR;
                }
            }
        }
    }
}

#endif // WORLD_H
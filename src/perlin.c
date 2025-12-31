#include "perlin.h"
#include <stdlib.h>
#include <math.h>

static inline uint64_t murmur64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccd;
    x ^= x >> 33;
    return x;
}

static inline float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static inline float lerp(float t, float a, float b) {
    return a + t * (b - a);
}

static inline float grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = (h < 8) ? x : y;
    float v = (h < 8) ? y : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

PerlinNoise* perlin_init(uint64_t seed) {
    PerlinNoise* pn = malloc(sizeof(PerlinNoise));
    
    for (int i = 0; i < 256; i++) {
        uint64_t h = murmur64((uint64_t)i ^ seed);
        pn->perm[i] = h & 255;
        pn->perm[i + 256] = pn->perm[i];
    }
    
    return pn;
}

void perlin_free(PerlinNoise* pn) {
    free(pn);
}

float perlin_noise3d(PerlinNoise* pn, float x, float y, float z) {
    int xi = (int)floorf(x) & 255;
    int yi = (int)floorf(y) & 255;
    int zi = (int)floorf(z) & 255;
    
    float xf = x - floorf(x);
    float yf = y - floorf(y);
    float zf = z - floorf(z);
    
    float u = fade(xf);
    float v = fade(yf);
    float w = fade(zf);
    
    uint8_t* p = pn->perm;
    int aa = p[p[xi] + yi];
    int ab = p[p[xi] + (yi + 1) & 255];
    int ba = p[p[(xi + 1) & 255] + yi];
    int bb = p[p[(xi + 1) & 255] + (yi + 1) & 255];
    
    float g0 = grad(p[aa + zi], xf, yf, zf);
    float g1 = grad(p[ba + zi], xf - 1, yf, zf);
    float g2 = grad(p[ab + zi], xf, yf - 1, zf);
    float g3 = grad(p[bb + zi], xf - 1, yf - 1, zf);
    float g4 = grad(p[aa + (zi + 1) & 255], xf, yf, zf - 1);
    float g5 = grad(p[ba + (zi + 1) & 255], xf - 1, yf, zf - 1);
    float g6 = grad(p[ab + (zi + 1) & 255], xf, yf - 1, zf - 1);
    float g7 = grad(p[bb + (zi + 1) & 255], xf - 1, yf - 1, zf - 1);
    
    float l0 = lerp(u, g0, g1);
    float l1 = lerp(u, g2, g3);
    float l2 = lerp(u, g4, g5);
    float l3 = lerp(u, g6, g7);
    
    float l4 = lerp(v, l0, l1);
    float l5 = lerp(v, l2, l3);
    
    return lerp(w, l4, l5);
}

float perlin_octave(PerlinNoise* pn, float x, float y, float z, int octaves, float persistence) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float max_value = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        value += perlin_noise3d(pn, x * frequency, y * frequency, z * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }
    
    return value / max_value;
}

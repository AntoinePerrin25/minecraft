#ifndef MC_PERLIN_H
#define MC_PERLIN_H

#include <stdint.h>

typedef struct {
    uint8_t perm[512];
} PerlinNoise;

PerlinNoise *perlin_init(uint64_t seed);
void perlin_free(PerlinNoise *pn);
float perlin_noise3d(PerlinNoise *pn, float x, float y, float z);
float perlin_octave(PerlinNoise *pn, float x, float y, float z, int octaves, float persistence);

#endif

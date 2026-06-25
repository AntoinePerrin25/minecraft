#ifndef MC_BLOCK_H
#define MC_BLOCK_H

#include <stdint.h>

typedef enum {
    BLOCK_NONE,
    BLOCK_AIR,
    BLOCK_BEDROCK,
    BLOCK_DIRT,
    BLOCK_GRASS,
    BLOCK_STONE,
    BLOCK_WATER,
    BLOCK_SAND,
    BLOCK_WOOD,
    BLOCK_NULL,
    BLOCK_BREAKING
} BlockType;

typedef struct __attribute__((packed, aligned(1))) BlockData {
    uint16_t Type       : 9;
    uint16_t lightLevel : 4;
    uint16_t gravity    : 1;
    uint16_t solid      : 1;
    uint16_t visible    : 1;
} BlockData;

BlockData createBlock(BlockType type);

#endif

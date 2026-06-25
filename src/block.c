#include "block.h"

#include <stdio.h>
#include <stdlib.h>

BlockData createBlock(BlockType type)
{
    BlockData blockData;
    blockData.Type = type;
    switch (type) {
    case BLOCK_NONE:
    case BLOCK_AIR:
        blockData.lightLevel = 0;
        blockData.gravity = 0;
        blockData.solid = 0;
        blockData.visible = 0;
        break;

    case BLOCK_BEDROCK:
        blockData.lightLevel = 0;
        blockData.gravity = 0;
        blockData.solid = 1;
        blockData.visible = 1;
        break;

    case BLOCK_DIRT:
    case BLOCK_GRASS:
    case BLOCK_STONE:
    case BLOCK_WOOD:
        blockData.lightLevel = 0;
        blockData.gravity = 0;
        blockData.solid = 1;
        blockData.visible = 1;
        break;

    case BLOCK_WATER:
        blockData.lightLevel = 0;
        blockData.gravity = 0;
        blockData.solid = 0;
        blockData.visible = 1;
        break;

    case BLOCK_SAND:
        blockData.lightLevel = 0;
        blockData.gravity = 1;
        blockData.solid = 1;
        blockData.visible = 1;
        break;

    default:
        fprintf(stderr, "createBlock: Unknown block type %d\n", type);
        exit(1);
        break;
    }
    return blockData;
}

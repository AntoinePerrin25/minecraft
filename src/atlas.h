#ifndef ATLAS_H
#define ATLAS_H

#include "raylib.h"

#define ATLAS_WIDTH 256
#define ATLAS_HEIGHT 256
#define BLOCK_TEXTURE_SIZE 16
#define ATLAS_COLS 16  // 256 / 16
#define ATLAS_ROWS 16  // 256 / 16

// Enum pour les textures dans l'atlas
typedef enum {
    ATLAS_GRASS_TOP,
    ATLAS_STONE,
    ATLAS_DIRT,
    ATLAS_GRASS_SIDE,
    ATLAS_WOOD_SIDE,
    ATLAS_STONE_SLAB_SIDE,
    ATLAS_STONE_SLAB_TOP,
    ATLAS_BRICK,
    ATLAS_TNT_SIDE,
    ATLAS_TNT_TOP,
    ATLAS_TNT_BOT,
    ATLAS_COBWEB,
    ATLAS_POPPY,
    ATLAS_DANDELION,
    ATLAS_WATER,
    ATLAS_OAK_SAPPLING,
    ATLAS_COBBLE,
    ATLAS_BEDROCK,
    ATLAS_SAND,
    ATLAS_GRAVEL,
    ATLAS_OAK_LOG_SIDE,
    ATLAS_OAK_LOG_TOP,
    ATLAS_IRON_BLOCK,
    ATLAS_GOLD_BLOCK,
    ATLAS_DIAMOND_BLOCK,
    ATLAS_EMERALD_BLOCK,
    ATLAS_REDSTONE_BLOCK,
    ATLAS_NULL1,
    ATLAS_RED_MUSHROOM,
    ATLAS_BROWN_MUSHROOM,
    ATLAS_JUNGLE_SAPPLING,
    ATLAS_FIRE,
    ATLAS_GOLD_ORE,
    ATLAS_IRON_ORE,
    ATLAS_COAL_ORE,
    ATLAS_BOOKSHELF,
    ATLAS_MOSSY_COBBLE,
    ATLAS_OBSIDIAN,
    ATLAS_NULL2,
    ATLAS_FERN,
    ATLAS_GRASS_BIOME,
    ATLAS_COUNT
} AtlasTexture;

// Structure pour définir les textures de chaque face d'un bloc
typedef struct {
    int top;      // Face supérieure (+Y)
    int bottom;   // Face inférieure (-Y)
    int north;    // Face nord (-Z)
    int south;    // Face sud (+Z)
    int east;     // Face est (+X)
    int west;     // Face ouest (-X)
} BlockFaceTextures;

// Fonctions pour gérer l'atlas
Texture2D LoadAtlasTexture(const char* filepath);
Rectangle GetTextureRectFromAtlas(int atlasIndex);
BlockFaceTextures GetBlockTextures(int blockType);
int GetBlockFaceTexture(int blockType, int faceIndex);

#endif
#include "atlas.h"
#include "data.h"
#include "raylib.h"
#include <stdio.h>

// Mapping des types de blocs aux textures de leurs faces
// Format: {top, bottom, north, south, east, west}
// Pour un bloc uniforme (comme la pierre), toutes les faces sont identiques
static const BlockFaceTextures blockTextureMap[] = {
    [BLOCK_NONE]    = {0, 0, 0, 0, 0, 0},  // Pas de texture
    [BLOCK_AIR]     = {0, 0, 0, 0, 0, 0},  // Pas de texture
    [BLOCK_BEDROCK] = {ATLAS_BEDROCK, ATLAS_BEDROCK, ATLAS_BEDROCK, ATLAS_BEDROCK, ATLAS_BEDROCK, ATLAS_BEDROCK},
    [BLOCK_DIRT]    = {ATLAS_DIRT, ATLAS_DIRT, ATLAS_DIRT, ATLAS_DIRT, ATLAS_DIRT, ATLAS_DIRT},
    [BLOCK_GRASS]   = {ATLAS_GRASS_TOP, ATLAS_DIRT, ATLAS_GRASS_SIDE, ATLAS_GRASS_SIDE, ATLAS_GRASS_SIDE, ATLAS_GRASS_SIDE},
    [BLOCK_STONE]   = {ATLAS_STONE, ATLAS_STONE, ATLAS_STONE, ATLAS_STONE, ATLAS_STONE, ATLAS_STONE},
    [BLOCK_WATER]   = {ATLAS_WATER, ATLAS_WATER, ATLAS_WATER, ATLAS_WATER, ATLAS_WATER, ATLAS_WATER},
    [BLOCK_SAND]    = {ATLAS_SAND, ATLAS_SAND, ATLAS_SAND, ATLAS_SAND, ATLAS_SAND, ATLAS_SAND},
    [BLOCK_WOOD]    = {ATLAS_OAK_LOG_TOP, ATLAS_OAK_LOG_TOP, ATLAS_OAK_LOG_SIDE, ATLAS_OAK_LOG_SIDE, ATLAS_OAK_LOG_SIDE, ATLAS_OAK_LOG_SIDE},
    [BLOCK_NULL]    = {ATLAS_NULL1, ATLAS_NULL1, ATLAS_NULL1, ATLAS_NULL1, ATLAS_NULL1, ATLAS_NULL1},
};

// Charger la texture atlas
Texture2D LoadAtlasTexture(const char* filepath)
{
    Texture2D atlas = LoadTexture(filepath);
    if (atlas.id == 0)
    {
        fprintf(stderr, "Erreur: Impossible de charger l'atlas de textures: %s\n", filepath);
    }
    else
    {
        // Configurer les paramètres de texture pour un rendu pixel-perfect
        SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
        SetTextureWrap(atlas, TEXTURE_WRAP_CLAMP);
        printf("Atlas chargé: %s (%dx%d)\n", filepath, atlas.width, atlas.height);
    }
    return atlas;
}

// Obtenir le rectangle UV pour une texture dans l'atlas
// atlasIndex: index de la texture (0-255 pour une grille 16x16)
Rectangle GetTextureRectFromAtlas(int atlasIndex)
{
    if (atlasIndex < 0)
    {
        atlasIndex = 0;
    }
    
    // Calculer la position dans la grille 16x16
    int row = atlasIndex / ATLAS_COLS;
    int col = atlasIndex % ATLAS_COLS;
    
    // Calculer les coordonnées UV normalisées (0.0 à 1.0)
    float u = (float)(col * BLOCK_TEXTURE_SIZE) / (float)ATLAS_WIDTH;
    float v = (float)(row * BLOCK_TEXTURE_SIZE) / (float)ATLAS_HEIGHT;
    float width = (float)BLOCK_TEXTURE_SIZE / (float)ATLAS_WIDTH;
    float height = (float)BLOCK_TEXTURE_SIZE / (float)ATLAS_HEIGHT;
    
    return (Rectangle){u, v, width, height};
}

// Obtenir les textures pour toutes les faces d'un type de bloc
BlockFaceTextures GetBlockTextures(int blockType)
{
    // Vérifier que le type de bloc est valide
    if (blockType < 0 || blockType >= (int)(sizeof(blockTextureMap) / sizeof(blockTextureMap[0])))
    {
        // Retourner une texture par défaut (pierre)
        return blockTextureMap[BLOCK_NULL];
    }
    
    return blockTextureMap[blockType];
}

// Fonction utilitaire pour obtenir la texture d'une face spécifique
int GetBlockFaceTexture(int blockType, int faceIndex)
{
    BlockFaceTextures textures = GetBlockTextures(blockType);
    
    // faceIndex: 0=droite(+X), 1=gauche(-X), 2=haut(+Y), 3=bas(-Y), 4=avant(+Z), 5=arrière(-Z)
    switch (faceIndex)
    {
        case 0: return textures.east;    // Droite (+X)
        case 1: return textures.west;    // Gauche (-X)
        case 2: return textures.top;     // Haut (+Y)
        case 3: return textures.bottom;  // Bas (-Y)
        case 4: return textures.south;   // Avant (+Z)
        case 5: return textures.north;   // Arrière (-Z)
        default: return ATLAS_STONE;     // Fallback
    }
}

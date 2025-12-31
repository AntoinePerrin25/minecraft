#include "data.h"
#include "atlas.h"
#include "perlin.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Variable globale pour le nombre total de chunks
static int g_totalChunks = 0;
static PerlinNoise* g_terrain_noise = NULL;
static PerlinNoise* g_detail_noise = NULL;

void setGlobalChunkCount(int count) {
    g_totalChunks = count;
}

void initGenerators(uint64_t seed) {
    if (g_terrain_noise) perlin_free(g_terrain_noise);
    if (g_detail_noise) perlin_free(g_detail_noise);
    g_terrain_noise = perlin_init(seed);
    g_detail_noise = perlin_init(seed ^ 0xdeadbeef);
}

typedef struct {
    Vector3 normal;     // Normale de la face (vers l'extérieur)
    int offset[3];      // Décalage pour le voisin
    Color color;        // Couleur de la face (peut varier si texture)
} BlockFace;

static const BlockFace blockFaces[6] = {
    {{ 1,  0,  0}, { 1,  0,  0}, {0}}, // Droite
    {{-1,  0,  0}, {-1,  0,  0}, {0}}, // Gauche
    {{ 0,  1,  0}, { 0,  1,  0}, {0}}, // Haut
    {{ 0, -1,  0}, { 0, -1,  0}, {0}}, // Bas
    {{ 0,  0,  1}, { 0,  0,  1}, {0}}, // Avant
    {{ 0,  0, -1}, { 0,  0, -1}, {0}},  // Arrière
};

BlockData createBlock(BlockType type)
{
    BlockData blockData;
    blockData.Type = type;
    switch (type)
    {
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
        blockData.gravity = 1; // sand falls
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

void generateChunk(Chunk *chunk, int chunkX, int chunkZ) {
    chunk->x = chunkX;
    chunk->z = chunkZ;
    
    if (!g_terrain_noise) return;
    
    // Coordonnées monde du chunk (en blocs)
    int worldXBase = chunkX << 4;
    int worldZBase = chunkZ << 4;
    
    // Génération avec 2 octaves pour variation rapide + detail
    for (int lx = 0; lx < 16; lx++) {
        for (int lz = 0; lz < 16; lz++) {
            int wx = worldXBase + lx;
            int wz = worldZBase + lz;
            
            // Bruit de base pour la hauteur (échelle large)
            float terrain = perlin_octave(g_terrain_noise, wx * 0.01f, 0, wz * 0.01f, 3, 0.5f);
            
            // Bruit de détail pour variation locale (plus petite échelle)
            float detail = perlin_noise3d(g_detail_noise, wx * 0.05f, 0, wz * 0.05f);
            
            // Combiner: 80% terrain, 20% detail
            float height_factor = terrain * 0.8f + detail * 0.2f;
            
            // Convertir en hauteur de terrain (40-80 blocs)
            int max_height = 40 + (int)((height_factor + 1.0f) * 20.0f);
            max_height = max_height < 5 ? 5 : (max_height > 100 ? 100 : max_height);
            
            // Remplir colonne verticale
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

// Fonction pour convertir des coordonnées monde en coordonnées de bloc
BlockInWorld worldToBlockCoords(Vector3 worldPos)
{
    BlockInWorld biw;
    biw.blockCoord.x = (int)floorf(worldPos.x) & 15; // Modulo 16 pour obtenir la coordonnée dans le chunk
    biw.blockCoord.y = (int)floorf(worldPos.y);
    biw.blockCoord.z = (int)floorf(worldPos.z) & 15; // Modulo 16 pour obtenir la coordonnée dans le chunk
    biw.chunkCoord.x = (int)floorf(worldPos.x) >> 4; // Division par 16 pour obtenir la coordonnée du chunk
    biw.chunkCoord.z = (int)floorf(worldPos.z) >> 4; // Division par 16 pour obtenir la coordonnée du chunk
    return biw;
}

BlockData getBlockAt(Chunk *chunks, int worldX, int worldY, int worldZ)
{
    // Guard Y bounds
    if (worldY < 0 || worldY >= WORLD_HEIGHT)
    {
        return createBlock(BLOCK_AIR);
    }

    // Calcul correct des coordonnées de chunk (gestion des coordonnées négatives)
    int chunkX = (worldX >= 0) ? (worldX >> 4) : ((worldX + 1) / 16 - 1);
    int chunkZ = (worldZ >= 0) ? (worldZ >> 4) : ((worldZ + 1) / 16 - 1);
    
    // Calcul correct des coordonnées locales (gestion des coordonnées négatives)
    int localX = worldX - (chunkX * 16);
    int localZ = worldZ - (chunkZ * 16);
    
    // Sécurité : vérifier que les coordonnées locales sont dans les limites
    if (localX < 0 || localX >= 16 || localZ < 0 || localZ >= 16)
    {
        return createBlock(BLOCK_AIR);
    }

    // Trouver le chunk correspondant en parcourant tous les chunks actifs
    for (int i = 0; i < g_totalChunks; i++)
    {
        if (chunks[i].active && chunks[i].x == chunkX && chunks[i].z == chunkZ)
        {
            return chunks[i].data.blocks[localX][worldY][localZ];
        }
    }

    // Si le chunk n'est pas trouvé, retourner un bloc AIR
    return createBlock(BLOCK_AIR);
}

// Get neighboring block positions
int isBlockExposed(Chunk *chunks, int x, int y, int z)
{
    // Check all 6 neighboring blocks
    int exposed = 0;
    int offsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    for (int i = 0; i < 6; i++)
    {
        int nx = x + offsets[i][0];
        int ny = y + offsets[i][1];
        int nz = z + offsets[i][2];
        BlockData neighbor = getBlockAt(chunks, nx, ny, nz);
        if (neighbor.Type == BLOCK_AIR || neighbor.Type == BLOCK_NONE)
        {
            exposed = 1;
            break;
        }
    }

    return exposed;
}
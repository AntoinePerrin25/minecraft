#include "data.h"
#include "atlas.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

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

void generateChunk(Chunk *chunk, int chunkX, int chunkZ)
{
    chunk->x = chunkX;
    chunk->z = chunkZ;
    // Pour l'instant, générons un terrain plat simple
    for (int x = 0; x < 16; x++)
    {
        for (int z = 0; z < 16; z++)
        {
            for (int y = 0; y < 128; y++)
            {
                if (y > 64)
                {
                    chunk->data.blocks[x][y][z] = createBlock(BLOCK_AIR);
                }
                else if (y == 64)
                {
                    chunk->data.blocks[x][y][z] = createBlock(BLOCK_GRASS);
                }
                else if (y >= 60)
                {
                    chunk->data.blocks[x][y][z] = createBlock(BLOCK_DIRT);
                }
                else if (y >= 4)
                {
                    chunk->data.blocks[x][y][z] = createBlock(BLOCK_STONE);
                }
                else
                {
                    chunk->data.blocks[x][y][z] = createBlock(BLOCK_BEDROCK);
                }
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

    // Trouver le chunk correspondant
    for (int i = 0; i < (2 * RENDER_DISTANCE + 1) * (2 * RENDER_DISTANCE + 1); i++)
    {
        if (chunks[i].x == chunkX && chunks[i].z == chunkZ)
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
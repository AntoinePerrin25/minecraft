#ifndef CHUNK_MESH_H
#define CHUNK_MESH_H
#include <string.h>

#include "raylib.h"
#include "raymath.h"
#include "network.h"
#include "rlgl.h"


#define CHUNK_SIZE 16
#define WORLD_HEIGHT 256


typedef struct ChunkMesh
{
    Vector3 *vertices;
    Vector3 *normals; // Added normal vectors
    Color *colors;
    int *indices;
    int vertexCount;
    int indexCount;
    int capacity;
    Mesh mesh;
    bool dirty;
    bool initialized;
} ChunkMesh;

// Helper function to get a block type from FullChunk
static BlockType getBlockType(FullChunk *chunk, int x, int y, int z)
{
    // Check bounds
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= WORLD_HEIGHT || z < 0 || z >= CHUNK_SIZE)
        return BLOCK_AIR; // Out of bounds is treated as air
    
    return chunk->blocks[y][x][z].Type;
}

// Get block data properties
static bool isBlockSolid(BlockType blockType)
{
    return blockType != BLOCK_AIR && blockType != BLOCK_WATER;
}

// Check if a face is visible (block next to the face is transparent or air)
static bool isFaceVisible(FullChunk *chunk, int x, int y, int z, int faceIndex)
{
    // Offsets for adjacent blocks: -X, +X, -Y, +Y, -Z, +Z
    const int offsets[6][3] = {
        {-1, 0, 0}, {1, 0, 0}, 
        {0, -1, 0}, {0, 1, 0}, 
        {0, 0, -1}, {0, 0, 1}
    };
    
    int nx = x + offsets[faceIndex][0];
    int ny = y + offsets[faceIndex][1];
    int nz = z + offsets[faceIndex][2];
    
    BlockType adjacentBlockType = getBlockType(chunk, nx, ny, nz);
    
    // Check if the adjacent block is air or transparent
    return adjacentBlockType == BLOCK_AIR || adjacentBlockType == BLOCK_WATER;
}

// Function to add vertices and indices for a face
static void addFace(ChunkMesh *mesh, int faceIndex, int x, int y, int z, Color color)
{
    // Ensure we have enough capacity
    if (mesh->vertexCount + 4 > mesh->capacity)
    {
        int newCapacity = mesh->capacity == 0 ? 1024 : mesh->capacity * 2;
        mesh->vertices = realloc(mesh->vertices, newCapacity * sizeof(Vector3));
        mesh->normals = realloc(mesh->normals, newCapacity * sizeof(Vector3));
        mesh->colors = realloc(mesh->colors, newCapacity * sizeof(Color));
        mesh->indices = realloc(mesh->indices, newCapacity * 6 * sizeof(int)); // 6 indices per quad
        mesh->capacity = newCapacity;
    }

    // Define the 6 faces of a cube (front, back, left, right, top, bottom)
    float vertices[6][4][3] = {
        // Front face (-Z)
        {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}},
        // Back face (+Z)
        {{0, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}},
        // Left face (-X)
        {{0, 0, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}},
        // Right face (+X)
        {{1, 0, 0}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}},
        // Top face (+Y)
        {{0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}},
        // Bottom face (-Y)
        {{0, 0, 0}, {0, 0, 1}, {1, 0, 1}, {1, 0, 0}}
    };
    
    // Define normal vectors for each face
    Vector3 normals[6] = {
        {0, 0, -1},  // Front face (-Z)
        {0, 0, 1},   // Back face (+Z)
        {-1, 0, 0},  // Left face (-X)
        {1, 0, 0},   // Right face (+X)
        {0, 1, 0},   // Top face (+Y)
        {0, -1, 0}   // Bottom face (-Y)
    };

    // Add the 4 vertices of the face
    int baseIndex = mesh->vertexCount;
    for (int i = 0; i < 4; i++)
    {
        mesh->vertices[baseIndex + i] = (Vector3){
            x + vertices[faceIndex][i][0],
            y + vertices[faceIndex][i][1],
            z + vertices[faceIndex][i][2]
        };
        mesh->normals[baseIndex + i] = normals[faceIndex];
        mesh->colors[baseIndex + i] = color;
    }

    // Add indices for the 2 triangles that make up the face
    int indexBase = mesh->indexCount;
    mesh->indices[indexBase + 0] = baseIndex + 0;
    mesh->indices[indexBase + 1] = baseIndex + 1;
    mesh->indices[indexBase + 2] = baseIndex + 2;
    mesh->indices[indexBase + 3] = baseIndex + 0;
    mesh->indices[indexBase + 4] = baseIndex + 2;
    mesh->indices[indexBase + 5] = baseIndex + 3;

    mesh->vertexCount += 4;
    mesh->indexCount += 6;
}

// Update the chunk mesh based on the chunk data
static void updateChunkMesh(ChunkMesh *mesh, FullChunk *chunk)
{
    // Libérer les anciens buffers GPU et CPU
    if (mesh->initialized)
    {
        UnloadMesh(mesh->mesh);
        mesh->initialized = false;
    }

    if (mesh->vertices)
    {
        free(mesh->vertices);
        free(mesh->normals);
        free(mesh->colors);
        free(mesh->indices);
        mesh->vertices = NULL;
        mesh->normals = NULL;
        mesh->colors = NULL;
        mesh->indices = NULL;
        mesh->capacity = 0;
    }

    // Réinitialiser le mesh
    mesh->vertexCount = 0;
    mesh->indexCount = 0;

    // Pour chaque block dans le chunk
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        for (int y = 0; y < WORLD_HEIGHT; y++)
        {
            for (int z = 0; z < CHUNK_SIZE; z++)
            {
                BlockType blockType = getBlockType(chunk, x, y, z);
                
                // Skip air blocks
                if (blockType == BLOCK_AIR)
                    continue;
                    
                // Déterminer la couleur du bloc
                Color color;
                switch (blockType)
                {
                case BLOCK_BEDROCK:
                    color = BLACK;
                    break;
                case BLOCK_STONE:
                    color = DARKGRAY;
                    break;
                case BLOCK_DIRT:
                    color = BROWN;
                    break;
                case BLOCK_GRASS:
                    color = GREEN;
                    break;
                case BLOCK_SAND:
                    color = GOLD;
                    break;
                case BLOCK_WATER:
                    color = BLUE;
                    break;
                case BLOCK_WOOD:
                    color = DARKBROWN;
                    break;
                default:
                    color = WHITE;
                }
                    
                // Add a random variation to the color
                float shade = 0.85f + ((float)(rand() % 20) / 100.0f);
                color.r = (unsigned char)(color.r * shade);
                color.g = (unsigned char)(color.g * shade);
                color.b = (unsigned char)(color.b * shade);
                
                // Check each face
                for (int face = 0; face < 6; face++)
                {
                    if (isFaceVisible(chunk, x, y, z, face))
                    {
                        addFace(mesh, face, x, y, z, color);
                    }
                }
            }
        }
    }
    
    // Generate the mesh if there are vertices
    if (mesh->vertexCount > 0)
    {
        mesh->mesh = LoadMeshEx(
            mesh->vertices, mesh->vertexCount, 
            NULL,         // texcoords 
            NULL,         // texcoords2
            mesh->normals, 
            mesh->colors,
            mesh->indices, mesh->indexCount
        );
        
        mesh->initialized = true;
    }
    
    mesh->dirty = false;
}

#endif // CHUNK_MESH_H
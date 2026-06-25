#ifndef MC_CHUNK_H
#define MC_CHUNK_H

#include "block.h"
#include "config.h"
#include "raylib.h"

typedef struct __attribute__((packed)) ChunkData {
    int8_t ChunkHeight;
    BlockData blocks[CHUNK_SIZE][WORLD_HEIGHT][CHUNK_SIZE];
} ChunkData;

typedef struct ChunkRenderData {
    unsigned int vao;
    unsigned int vbo;
    unsigned int ibo;
    int indexCount;
    int vertexCount;
    void *cpuVertices;
    void *cpuIndices;
    int needsRemesh;
    int meshing;
    int meshReady;
    float aabbMin[3];
    float aabbMax[3];
    void *user;
    int hasMesh;
    Mesh mesh;
} ChunkRenderData;

typedef struct Chunk {
    int x;
    int z;
    ChunkData data;
    ChunkRenderData render;
    int loaded;
    int active;
} Chunk;

#endif

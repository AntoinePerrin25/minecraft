#include "mesh.h"
#include "atlas.h"
#include "data.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Simple job / ready queues
typedef struct MeshJob {
    int chunkIndex;
    int priority;
    struct MeshJob *next;
} MeshJob;

typedef struct ReadyMesh {
    int chunkIndex;
    float *positions; // x,y,z * vertexCount
    float *normals;   // nx,ny,nz * vertexCount
    float *texcoords; // u,v * vertexCount
    unsigned int *indices;
    int vertexCount;
    int indexCount;
    struct ReadyMesh *next;
} ReadyMesh;

static MeshJob *jobHead = NULL;
static ReadyMesh *readyHead = NULL;
static pthread_mutex_t jobMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t jobCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t readyMutex = PTHREAD_MUTEX_INITIALIZER;

static Chunk *g_chunks = NULL;
static int g_totalChunks = 0;
static int g_shutdown = 0;
static pthread_t workerThread;
static Texture2D g_atlas = {0};
static Material g_material = {0};

// Utility to push job (no sorting for simplicity, but could be improved)
static void push_job(int chunkIndex, int priority) {
    MeshJob *j = malloc(sizeof(MeshJob));
    j->chunkIndex = chunkIndex;
    j->priority = priority;
    j->next = NULL;
    pthread_mutex_lock(&jobMutex);
    // simple push at head
    j->next = jobHead;
    jobHead = j;
    pthread_cond_signal(&jobCond);
    pthread_mutex_unlock(&jobMutex);
}

static MeshJob *pop_job(void) {
    pthread_mutex_lock(&jobMutex);
    while (!jobHead && !g_shutdown) {
        pthread_cond_wait(&jobCond, &jobMutex);
    }
    if (g_shutdown) {
        pthread_mutex_unlock(&jobMutex);
        return NULL;
    }
    MeshJob *j = jobHead;
    jobHead = jobHead->next;
    pthread_mutex_unlock(&jobMutex);
    return j;
}

static void push_ready(ReadyMesh *r) {
    pthread_mutex_lock(&readyMutex);
    r->next = readyHead;
    readyHead = r;
    pthread_mutex_unlock(&readyMutex);
}

static ReadyMesh *pop_ready(void) {
    pthread_mutex_lock(&readyMutex);
    ReadyMesh *r = readyHead;
    if (r) readyHead = r->next;
    pthread_mutex_unlock(&readyMutex);
    return r;
}

// Basic face-culling mesher (no greedy) for simplicity and correctness.
// It generates quads for each exposed face.
// vertices layout per vertex: x,y,z, nx,ny,nz, u,v (8 floats)
static ReadyMesh *mesh_chunk_improved(int chunkIndex) {
    Chunk *chunk = &g_chunks[chunkIndex];
    // We'll implement greedy merging for top faces (Y axis), and keep simple
    // per-face meshing for vertical faces. This provides a large win for terrain.
    int vcap = 16384;
    int icap = 32768;
    float *positions = malloc(sizeof(float) * 3 * vcap);
    float *normals = malloc(sizeof(float) * 3 * vcap);
    float *texcoords = malloc(sizeof(float) * 2 * vcap);
    unsigned int *indices = malloc(sizeof(unsigned int) * icap);
    int vcount = 0;
    int icount = 0;

    // Helper to ensure capacity
    #define ENSURE_CAP(addV, addI) do { \
        if (vcount + (addV) > vcap) { vcap *= 2; positions = realloc(positions, sizeof(float)*3*vcap); normals = realloc(normals, sizeof(float)*3*vcap); texcoords = realloc(texcoords, sizeof(float)*2*vcap); } \
        if (icount + (addI) > icap) { icap *= 2; indices = realloc(indices, sizeof(unsigned int)*icap); } \
    } while(0)

    // Greedy on top faces (+Y)
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        // build mask for this y where top face is exposed (block at y and above is air)
        int mask[CHUNK_SIZE][CHUNK_SIZE];
        int texmap[CHUNK_SIZE][CHUNK_SIZE];
        int any = 0;
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                BlockData b = chunk->data.blocks[x][y][z];
                BlockData above = getBlockAt(g_chunks, (chunk->x<<4)+x, y+1, (chunk->z<<4)+z);
                if (b.visible && b.Type != BLOCK_AIR && (!above.visible || above.Type == BLOCK_AIR)) {
                    mask[x][z] = 1;
                    texmap[x][z] = GetBlockFaceTexture(b.Type, 2); // top face
                    any = 1;
                } else {
                    mask[x][z] = 0;
                }
            }
        }
        if (!any) continue;
        // Greedy merge rectangles in mask
        for (int z0 = 0; z0 < CHUNK_SIZE; z0++) {
            for (int x0 = 0; x0 < CHUNK_SIZE; ) {
                if (!mask[x0][z0]) { x0++; continue; }
                int tex = texmap[x0][z0];
                int w = 1;
                while (x0 + w < CHUNK_SIZE && mask[x0+w][z0] && texmap[x0+w][z0] == tex) w++;
                int h = 1;
                int ok = 1;
                while (z0 + h < CHUNK_SIZE && ok) {
                    for (int xi = 0; xi < w; xi++) {
                        if (!mask[x0+xi][z0+h] || texmap[x0+xi][z0+h] != tex) { ok = 0; break; }
                    }
                    if (ok) h++; else break;
                }
                // emit quad for rectangle covering x0..x0+w, z0..z0+h at y+1
                float ax = (float)x0;
                float az = (float)z0;
                float bx = (float)(x0 + w);
                float bz = (float)(z0 + h);
                float yplane = (float)(y + 1);
                // corners: (bx,yplane,az), (bx,yplane,bz), (ax,yplane,bz), (ax,yplane,az)
                ENSURE_CAP(4,6);
                // v0
                positions[vcount*3 + 0] = bx + (chunk->x<<4);
                positions[vcount*3 + 1] = yplane;
                positions[vcount*3 + 2] = az + (chunk->z<<4);
                normals[vcount*3 + 0] = 0; normals[vcount*3 + 1] = 1; normals[vcount*3 + 2] = 0;
                Rectangle uv = GetTextureRectFromAtlas(tex);
                // Utiliser une seule tuile de texture (pas de répétition)
                texcoords[vcount*2 + 0] = uv.x; texcoords[vcount*2 + 1] = uv.y + uv.height;
                // v1
                positions[vcount*3 + 3] = bx + (chunk->x<<4);
                positions[vcount*3 + 4] = yplane;
                positions[vcount*3 + 5] = bz + (chunk->z<<4);
                normals[vcount*3 + 3] = 0; normals[vcount*3 + 4] = 1; normals[vcount*3 + 5] = 0;
                texcoords[vcount*2 + 2] = uv.x; texcoords[vcount*2 + 3] = uv.y;
                // v2
                positions[vcount*3 + 6] = ax + (chunk->x<<4);
                positions[vcount*3 + 7] = yplane;
                positions[vcount*3 + 8] = bz + (chunk->z<<4);
                normals[vcount*3 + 6] = 0; normals[vcount*3 + 7] = 1; normals[vcount*3 + 8] = 0;
                texcoords[vcount*2 + 4] = uv.x + uv.width; texcoords[vcount*2 + 5] = uv.y;
                // v3
                positions[vcount*3 + 9] = ax + (chunk->x<<4);
                positions[vcount*3 + 10] = yplane;
                positions[vcount*3 + 11] = az + (chunk->z<<4);
                normals[vcount*3 + 9] = 0; normals[vcount*3 + 10] = 1; normals[vcount*3 + 11] = 0;
                texcoords[vcount*2 + 6] = uv.x; texcoords[vcount*2 + 7] = uv.y + uv.height;
                indices[icount++] = vcount + 0; indices[icount++] = vcount + 2; indices[icount++] = vcount + 1;
                indices[icount++] = vcount + 0; indices[icount++] = vcount + 3; indices[icount++] = vcount + 2;
                vcount += 4;
                // clear mask
                for (int zz = 0; zz < h; zz++) for (int xx = 0; xx < w; xx++) mask[x0+xx][z0+zz] = 0;
                x0 += w;
            }
        }
    }

    // Greedy on bottom faces (-Y)
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        int mask[CHUNK_SIZE][CHUNK_SIZE];
        int texmap[CHUNK_SIZE][CHUNK_SIZE];
        int any = 0;
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                BlockData b = chunk->data.blocks[x][y][z];
                BlockData below = getBlockAt(g_chunks, (chunk->x<<4)+x, y-1, (chunk->z<<4)+z);
                if (b.visible && b.Type != BLOCK_AIR && (!below.visible || below.Type == BLOCK_AIR)) {
                    mask[x][z] = 1;
                    texmap[x][z] = GetBlockFaceTexture(b.Type, 3); // bottom face
                    any = 1;
                } else {
                    mask[x][z] = 0;
                }
            }
        }
        if (!any) continue;
        for (int z0 = 0; z0 < CHUNK_SIZE; z0++) {
            for (int x0 = 0; x0 < CHUNK_SIZE; ) {
                if (!mask[x0][z0]) { x0++; continue; }
                int tex = texmap[x0][z0];
                int w = 1;
                while (x0 + w < CHUNK_SIZE && mask[x0+w][z0] && texmap[x0+w][z0] == tex) w++;
                int h = 1;
                int ok = 1;
                while (z0 + h < CHUNK_SIZE && ok) {
                    for (int xi = 0; xi < w; xi++) {
                        if (!mask[x0+xi][z0+h] || texmap[x0+xi][z0+h] != tex) { ok = 0; break; }
                    }
                    if (ok) h++; else break;
                }
                float ax = (float)x0;
                float az = (float)z0;
                float bx = (float)(x0 + w);
                float bz = (float)(z0 + h);
                float yplane = (float)y;
                ENSURE_CAP(4,6);
                Rectangle uv = GetTextureRectFromAtlas(tex);
                // v0 - ordre inversé pour face bottom (normale vers bas)
                positions[vcount*3 + 0] = ax + (chunk->x<<4);
                positions[vcount*3 + 1] = yplane;
                positions[vcount*3 + 2] = az + (chunk->z<<4);
                normals[vcount*3 + 0] = 0; normals[vcount*3 + 1] = -1; normals[vcount*3 + 2] = 0;
                texcoords[vcount*2 + 0] = uv.x + uv.width; texcoords[vcount*2 + 1] = uv.y;
                // v1
                positions[vcount*3 + 3] = ax + (chunk->x<<4);
                positions[vcount*3 + 4] = yplane;
                positions[vcount*3 + 5] = bz + (chunk->z<<4);
                normals[vcount*3 + 3] = 0; normals[vcount*3 + 4] = -1; normals[vcount*3 + 5] = 0;
                texcoords[vcount*2 + 2] = uv.x + uv.width; texcoords[vcount*2 + 3] = uv.y + uv.height;
                // v2
                positions[vcount*3 + 6] = bx + (chunk->x<<4);
                positions[vcount*3 + 7] = yplane;
                positions[vcount*3 + 8] = bz + (chunk->z<<4);
                normals[vcount*3 + 6] = 0; normals[vcount*3 + 7] = -1; normals[vcount*3 + 8] = 0;
                texcoords[vcount*2 + 4] = uv.x; texcoords[vcount*2 + 5] = uv.y + uv.height;
                // v3
                positions[vcount*3 + 9] = bx + (chunk->x<<4);
                positions[vcount*3 + 10] = yplane;
                positions[vcount*3 + 11] = az + (chunk->z<<4);
                normals[vcount*3 + 9] = 0; normals[vcount*3 + 10] = -1; normals[vcount*3 + 11] = 0;
                texcoords[vcount*2 + 6] = uv.x; texcoords[vcount*2 + 7] = uv.y;
                indices[icount++] = vcount + 0; indices[icount++] = vcount + 2; indices[icount++] = vcount + 1;
                indices[icount++] = vcount + 0; indices[icount++] = vcount + 3; indices[icount++] = vcount + 2;
                vcount += 4;
                for (int zz = 0; zz < h; zz++) for (int xx = 0; xx < w; xx++) mask[x0+xx][z0+zz] = 0;
                x0 += w;
            }
        }
    }

    // For vertical faces (+X, -X, +Z, -Z) use simple per-face emission
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < WORLD_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                BlockData b = chunk->data.blocks[x][y][z];
                if (!b.visible || b.Type == BLOCK_AIR) continue;
                int worldX = (chunk->x << 4) + x;
                int worldZ = (chunk->z << 4) + z;
                // +X
                BlockData n = getBlockAt(g_chunks, worldX+1, y, worldZ);
                if (n.Type == BLOCK_AIR || !n.visible) {
                    ENSURE_CAP(4,6);
                    float px = x + 1 + (chunk->x<<4);
                    float py = y;
                    float pz = z + (chunk->z<<4);
                    // v0
                    positions[vcount*3 + 0] = px; positions[vcount*3 + 1] = py; positions[vcount*3 + 2] = pz;
                    normals[vcount*3 + 0] = 1; normals[vcount*3 + 1] = 0; normals[vcount*3 + 2] = 0;
                    Rectangle uv = GetTextureRectFromAtlas(GetBlockFaceTexture(b.Type, 0));
                    texcoords[vcount*2 + 0] = uv.x + uv.width; texcoords[vcount*2 + 1] = uv.y + uv.height;
                    // v1
                    positions[vcount*3 + 3] = px; positions[vcount*3 + 4] = py+1; positions[vcount*3 + 5] = pz;
                    normals[vcount*3 + 3] = 1; normals[vcount*3 + 4] = 0; normals[vcount*3 + 5] = 0;
                    texcoords[vcount*2 + 2] = uv.x + uv.width; texcoords[vcount*2 + 3] = uv.y;
                    // v2
                    positions[vcount*3 + 6] = px; positions[vcount*3 + 7] = py+1; positions[vcount*3 + 8] = pz+1;
                    normals[vcount*3 + 6] = 1; normals[vcount*3 + 7] = 0; normals[vcount*3 + 8] = 0;
                    texcoords[vcount*2 + 4] = uv.x; texcoords[vcount*2 + 5] = uv.y;
                    // v3
                    positions[vcount*3 + 9] = px; positions[vcount*3 + 10] = py; positions[vcount*3 + 11] = pz+1;
                    normals[vcount*3 + 9] = 1; normals[vcount*3 + 10] = 0; normals[vcount*3 + 11] = 0;
                    texcoords[vcount*2 + 6] = uv.x; texcoords[vcount*2 + 7] = uv.y + uv.height;
                    indices[icount++] = vcount + 0; indices[icount++] = vcount + 1; indices[icount++] = vcount + 2;
                    indices[icount++] = vcount + 0; indices[icount++] = vcount + 2; indices[icount++] = vcount + 3;
                    vcount += 4;
                }
                // -X
                n = getBlockAt(g_chunks, worldX-1, y, worldZ);
                if (n.Type == BLOCK_AIR || !n.visible) {
                    ENSURE_CAP(4,6);
                    float px = x + (chunk->x<<4);
                    float py = y;
                    float pz = z + (chunk->z<<4);
                    Rectangle uv = GetTextureRectFromAtlas(GetBlockFaceTexture(b.Type, 1));
                    positions[vcount*3 + 0] = px; positions[vcount*3 + 1] = py; positions[vcount*3 + 2] = pz+1;
                    normals[vcount*3 + 0] = -1; normals[vcount*3 + 1] = 0; normals[vcount*3 + 2] = 0;
                    texcoords[vcount*2 + 0] = uv.x + uv.width; texcoords[vcount*2 + 1] = uv.y + uv.height;
                    positions[vcount*3 + 3] = px; positions[vcount*3 + 4] = py+1; positions[vcount*3 + 5] = pz+1;
                    normals[vcount*3 + 3] = -1; normals[vcount*3 + 4] = 0; normals[vcount*3 + 5] = 0;
                    texcoords[vcount*2 + 2] = uv.x + uv.width; texcoords[vcount*2 + 3] = uv.y;
                    positions[vcount*3 + 6] = px; positions[vcount*3 + 7] = py+1; positions[vcount*3 + 8] = pz;
                    normals[vcount*3 + 6] = -1; normals[vcount*3 + 7] = 0; normals[vcount*3 + 8] = 0;
                    texcoords[vcount*2 + 4] = uv.x; texcoords[vcount*2 + 5] = uv.y;
                    positions[vcount*3 + 9] = px; positions[vcount*3 + 10] = py; positions[vcount*3 + 11] = pz;
                    normals[vcount*3 + 9] = -1; normals[vcount*3 + 10] = 0; normals[vcount*3 + 11] = 0;
                    texcoords[vcount*2 + 6] = uv.x; texcoords[vcount*2 + 7] = uv.y + uv.height;
                    indices[icount++] = vcount + 0; indices[icount++] = vcount + 1; indices[icount++] = vcount + 2;
                    indices[icount++] = vcount + 0; indices[icount++] = vcount + 2; indices[icount++] = vcount + 3;
                    vcount += 4;
                }
                // +Z
                n = getBlockAt(g_chunks, worldX, y, worldZ+1);
                if (n.Type == BLOCK_AIR || !n.visible) {
                    ENSURE_CAP(4,6);
                    float px = x + (chunk->x<<4);
                    float py = y;
                    float pz = z + 1 + (chunk->z<<4);
                    Rectangle uv = GetTextureRectFromAtlas(GetBlockFaceTexture(b.Type, 4));
                    positions[vcount*3 + 0] = px+1; positions[vcount*3 + 1] = py; positions[vcount*3 + 2] = pz;
                    normals[vcount*3 + 0] = 0; normals[vcount*3 + 1] = 0; normals[vcount*3 + 2] = 1;
                    texcoords[vcount*2 + 0] = uv.x + uv.width; texcoords[vcount*2 + 1] = uv.y + uv.height;
                    positions[vcount*3 + 3] = px+1; positions[vcount*3 + 4] = py+1; positions[vcount*3 + 5] = pz;
                    normals[vcount*3 + 3] = 0; normals[vcount*3 + 4] = 0; normals[vcount*3 + 5] = 1;
                    texcoords[vcount*2 + 2] = uv.x + uv.width; texcoords[vcount*2 + 3] = uv.y;
                    positions[vcount*3 + 6] = px; positions[vcount*3 + 7] = py+1; positions[vcount*3 + 8] = pz;
                    normals[vcount*3 + 6] = 0; normals[vcount*3 + 7] = 0; normals[vcount*3 + 8] = 1;
                    texcoords[vcount*2 + 4] = uv.x; texcoords[vcount*2 + 5] = uv.y;
                    positions[vcount*3 + 9] = px; positions[vcount*3 + 10] = py; positions[vcount*3 + 11] = pz;
                    normals[vcount*3 + 9] = 0; normals[vcount*3 + 10] = 0; normals[vcount*3 + 11] = 1;
                    texcoords[vcount*2 + 6] = uv.x; texcoords[vcount*2 + 7] = uv.y + uv.height;
                    indices[icount++] = vcount + 0; indices[icount++] = vcount + 1; indices[icount++] = vcount + 2;
                    indices[icount++] = vcount + 0; indices[icount++] = vcount + 2; indices[icount++] = vcount + 3;
                    vcount += 4;
                }
                // -Z
                n = getBlockAt(g_chunks, worldX, y, worldZ-1);
                if (n.Type == BLOCK_AIR || !n.visible) {
                    ENSURE_CAP(4,6);
                    float px = x + (chunk->x<<4);
                    float py = y;
                    float pz = z + (chunk->z<<4);
                    Rectangle uv = GetTextureRectFromAtlas(GetBlockFaceTexture(b.Type, 5));
                    positions[vcount*3 + 0] = px; positions[vcount*3 + 1] = py; positions[vcount*3 + 2] = pz;
                    normals[vcount*3 + 0] = 0; normals[vcount*3 + 1] = 0; normals[vcount*3 + 2] = -1;
                    texcoords[vcount*2 + 0] = uv.x + uv.width; texcoords[vcount*2 + 1] = uv.y + uv.height;
                    positions[vcount*3 + 3] = px; positions[vcount*3 + 4] = py+1; positions[vcount*3 + 5] = pz;
                    normals[vcount*3 + 3] = 0; normals[vcount*3 + 4] = 0; normals[vcount*3 + 5] = -1;
                    texcoords[vcount*2 + 2] = uv.x + uv.width; texcoords[vcount*2 + 3] = uv.y;
                    positions[vcount*3 + 6] = px+1; positions[vcount*3 + 7] = py+1; positions[vcount*3 + 8] = pz;
                    normals[vcount*3 + 6] = 0; normals[vcount*3 + 7] = 0; normals[vcount*3 + 8] = -1;
                    texcoords[vcount*2 + 4] = uv.x; texcoords[vcount*2 + 5] = uv.y;
                    positions[vcount*3 + 9] = px+1; positions[vcount*3 + 10] = py; positions[vcount*3 + 11] = pz;
                    normals[vcount*3 + 9] = 0; normals[vcount*3 + 10] = 0; normals[vcount*3 + 11] = -1;
                    texcoords[vcount*2 + 6] = uv.x; texcoords[vcount*2 + 7] = uv.y + uv.height;
                    indices[icount++] = vcount + 0; indices[icount++] = vcount + 1; indices[icount++] = vcount + 2;
                    indices[icount++] = vcount + 0; indices[icount++] = vcount + 2; indices[icount++] = vcount + 3;
                    vcount += 4;
                }
            }
        }
    }

    #undef ENSURE_CAP

    if (vcount == 0) {
        free(positions); free(normals); free(texcoords); free(indices);
        return NULL;
    }

    // shrink to fit
    positions = realloc(positions, sizeof(float)*3*vcount);
    normals = realloc(normals, sizeof(float)*3*vcount);
    texcoords = realloc(texcoords, sizeof(float)*2*vcount);
    indices = realloc(indices, sizeof(unsigned int)*icount);

    ReadyMesh *r = malloc(sizeof(ReadyMesh));
    r->chunkIndex = chunkIndex;
    r->positions = positions;
    r->normals = normals;
    r->texcoords = texcoords;
    r->indices = indices;
    r->vertexCount = vcount;
    r->indexCount = icount;
    r->next = NULL;
    return r;
}

// Worker thread
static void *worker_loop(void *arg) {
    (void)arg;
    while (!g_shutdown) {
        MeshJob *job = pop_job();
        if (!job) break;
        int idx = job->chunkIndex;
        free(job);
        // quick check
        if (idx < 0 || idx >= g_totalChunks) continue;
        // mark meshing
        g_chunks[idx].render.meshing = 1;
        ReadyMesh *result = mesh_chunk_improved(idx);
        if (result) push_ready(result);
        else {
            ReadyMesh *r = malloc(sizeof(ReadyMesh));
            r->chunkIndex = idx; r->positions = NULL; r->normals = NULL; r->texcoords = NULL; r->indices = NULL; r->vertexCount = 0; r->indexCount = 0; r->next = NULL; push_ready(r);
        }
    }
    return NULL;
}

void InitMeshSystem(Chunk* chunks, int totalChunks, Texture2D atlas) {
    g_chunks = chunks;
    g_totalChunks = totalChunks;
    g_shutdown = 0;
    g_atlas = atlas;
    // create default material and assign atlas
    g_material = LoadMaterialDefault();
    g_material.maps[MATERIAL_MAP_DIFFUSE].texture = atlas;
    // init render fields
    for (int i = 0; i < totalChunks; i++) {
        ChunkRenderData *r = &chunks[i].render;
        r->vao = 0; r->vbo = 0; r->ibo = 0;
        r->indexCount = 0; r->vertexCount = 0;
        r->cpuVertices = NULL; r->cpuIndices = NULL;
        r->needsRemesh = 1; r->meshing = 0; r->meshReady = 0;
        r->hasMesh = 0;
        float cx = (float)(chunks[i].x << 4);
        float cz = (float)(chunks[i].z << 4);
        r->aabbMin[0] = cx; r->aabbMin[1] = 0; r->aabbMin[2] = cz;
        r->aabbMax[0] = cx + CHUNK_SIZE; r->aabbMax[1] = WORLD_HEIGHT; r->aabbMax[2] = cz + CHUNK_SIZE;
    }
    // start worker
    pthread_create(&workerThread, NULL, worker_loop, NULL);
    // schedule initial remesh for all chunks
    for (int i = 0; i < totalChunks; i++) {
        ScheduleChunkRemesh(i, 0);
    }
}

void ShutdownMeshSystem(void) {
    // signal shutdown
    pthread_mutex_lock(&jobMutex);
    g_shutdown = 1;
    pthread_cond_signal(&jobCond);
    pthread_mutex_unlock(&jobMutex);
    pthread_join(workerThread, NULL);
    // free remaining jobs
    while (jobHead) { MeshJob *j = jobHead; jobHead = j->next; free(j); }
    // free ready meshes
    ReadyMesh *r;
    while ((r = pop_ready()) != NULL) {
        if (r->positions) free(r->positions);
        if (r->normals) free(r->normals);
        if (r->texcoords) free(r->texcoords);
        if (r->indices) free(r->indices);
        free(r);
    }
    // unload chunk meshes
    for (int i = 0; i < g_totalChunks; i++) {
        ChunkRenderData *rd = &g_chunks[i].render;
        if (rd->hasMesh) UnloadMesh(rd->mesh);
    }
    // unload material
    UnloadMaterial(g_material);
}

void ScheduleChunkRemesh(int chunkIndex, int priority) {
    if (chunkIndex < 0 || chunkIndex >= g_totalChunks) return;
    ChunkRenderData *r = &g_chunks[chunkIndex].render;
    if (r->meshing) return; // already in progress
    r->needsRemesh = 1;
    push_job(chunkIndex, priority);
}

// Called on main thread once per frame to upload a limited number of ready meshes
void PollMeshUploads(void) {
    const int uploadsPerFrame = 2; // tuning knob
    int uploads = 0;
    while (uploads < uploadsPerFrame) {
        ReadyMesh *r = pop_ready();
        if (!r) break;
        int idx = r->chunkIndex;
        ChunkRenderData *rd = &g_chunks[idx].render;
        // Upload must run on main thread. Use raylib Mesh helpers.
        if (r->vertexCount > 0 && r->indices) {
            // Build raylib Mesh from ready arrays. We transfer ownership of the
            // arrays to the Mesh so we do not free them here; UnloadMesh will
            // clean up later.
            Mesh mesh = {0};
            mesh.vertexCount = r->vertexCount;
            mesh.vertices = r->positions;
            mesh.normals = r->normals;
            mesh.texcoords = r->texcoords;
            mesh.triangleCount = r->indexCount / 3;
            // convert indices to unsigned short (raylib expects unsigned short*)
            unsigned short *sh_indices = malloc(sizeof(unsigned short) * r->indexCount);
            for (int i = 0; i < r->indexCount; i++) sh_indices[i] = (unsigned short)r->indices[i];
            mesh.indices = sh_indices;

            // Upload mesh to GPU (raylib function). Keep CPU data so Mesh can be used.
            UploadMesh(&mesh, false);

            // Store mesh in chunk render data
            if (rd->hasMesh) UnloadMesh(rd->mesh);
            rd->mesh = mesh;
            rd->hasMesh = 1;
            rd->indexCount = r->indexCount;
            rd->vertexCount = r->vertexCount;
            rd->meshReady = 1;

            // free r struct but NOT the arrays (now referenced by rd->mesh)
            free(r->indices);
            free(r);
        } else {
            // empty mesh case: mark as ready but no geometry
            rd->indexCount = 0;
            rd->vertexCount = 0;
            rd->meshReady = 1;
            rd->meshing = 0;
            free(r);
        }
        uploads++;
    }
}

// Simple AABB frustum culling using camera position + distance (cheap)
static int chunk_in_view(ChunkRenderData *r, Camera3D camera, Vector3 playerPos) {
    // cheap distance cull
    float cx = (r->aabbMin[0] + r->aabbMax[0]) * 0.5f;
    float cz = (r->aabbMin[2] + r->aabbMax[2]) * 0.5f;
    float dx = cx - playerPos.x;
    float dz = cz - playerPos.z;
    float dist2 = dx*dx + dz*dz;
    float maxDist = (RENDER_DISTANCE + 1) * CHUNK_SIZE;
    return dist2 <= (maxDist * maxDist);
}

void DrawChunks(Chunk* chunks, Camera3D camera, Vector3 playerPos) {
    int total = g_totalChunks;
    for (int i = 0; i < total; i++) {
        ChunkRenderData *r = &chunks[i].render;
        if (!r->meshReady) continue;
        if (r->indexCount == 0) continue;
        if (!chunk_in_view(r, camera, playerPos)) continue;
        // Draw stored mesh using shared material
        if (r->hasMesh) {
            Matrix transform = MatrixIdentity();
            DrawMesh(r->mesh, g_material, transform);
        }
    }
}

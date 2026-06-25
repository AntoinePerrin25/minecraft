#ifndef MC_MESH_H
#define MC_MESH_H

#include "chunk.h"
#include "raylib.h"

void InitMeshSystem(Chunk *chunks, int totalChunks, Texture2D atlas);
void ShutdownMeshSystem(void);
void ScheduleChunkRemesh(int chunkIndex, int priority);
void PollMeshUploads(void);
void DrawChunks(Chunk *chunks, Camera3D camera, Vector3 playerPos);
void UnloadChunkMesh(int chunkIndex);

#endif

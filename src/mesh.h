#ifndef MESH_H
#define MESH_H

#include "data.h"
#include "raylib.h"

void InitMeshSystem(Chunk* chunks, int totalChunks, Texture2D atlas);
void ShutdownMeshSystem(void);
void ScheduleChunkRemesh(int chunkIndex, int priority);
void PollMeshUploads(void);
void DrawChunks(Chunk* chunks, Camera3D camera, Vector3 playerPos);

#endif // MESH_H

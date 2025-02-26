#ifndef RENDERER_H
#define RENDERER_H

#ifdef _WIN32
#define WINDOWS_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#endif

#include "raylib.h"
#include "rlgl.h"

static void SetupRenderer(void) {
    // Enable depth test and back-face culling
    rlEnableDepthTest();
    rlEnableBackfaceCulling();
    
    // Set clear color
    ClearBackground(SKYBLUE);
}

// Wrapper functions pour éviter les conflits
static inline void RenderCube(Vector3 position, float width, float height, float length, Color color) {
    DrawCube(position, width, height, length, color);
}

static inline void RenderLine3D(Vector3 start, Vector3 end, Color color) {
    DrawLine3D(start, end, color);
}

static inline void RenderText(const char* text, int x, int y, int fontSize, Color color) {
    DrawText(text, x, y, fontSize, color);
}

#endif // RENDERER_H
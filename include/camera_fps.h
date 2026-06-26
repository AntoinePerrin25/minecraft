#ifndef MC_CAMERA_FPS_H
#define MC_CAMERA_FPS_H

#include "raylib.h"

typedef struct FpsLook {
    float yaw;
    float pitch;
} FpsLook;

typedef struct FpsBasis {
    Vector3 forward;
    Vector3 right;
    Vector3 up;
} FpsBasis;

void FpsLookInit(FpsLook *look);
void FpsLookApplyMouse(FpsLook *look, float mouseX, float mouseY);
FpsBasis FpsLookGetBasis(const FpsLook *look);
void FpsLookSyncCamera(const FpsLook *look, Vector3 position, Camera3D *camera);

#endif

#include "camera_fps.h"

#include "raymath.h"

#include <math.h>

static const float MAX_PITCH = 90.0f;

void FpsLookInit(FpsLook *look)
{
    look->yaw = 0.0f;
    look->pitch = 0.0f;
}

void FpsLookApplyMouse(FpsLook *look, float mouseX, float mouseY)
{
    look->yaw -= mouseX;
    look->pitch -= mouseY;

    if (look->pitch > MAX_PITCH) look->pitch = MAX_PITCH;
    if (look->pitch < -MAX_PITCH) look->pitch = -MAX_PITCH;
}

FpsBasis FpsLookGetBasis(const FpsLook *look)
{
    float yawRad = look->yaw * DEG2RAD;
    float pitchRad = look->pitch * DEG2RAD;
    float cp = cosf(pitchRad);
    float sp = sinf(pitchRad);
    float sy = sinf(yawRad);
    float cy = cosf(yawRad);

    FpsBasis basis;

    if (fabsf(cp) < 1e-6f) {
        float sign = (sp >= 0.0f) ? 1.0f : -1.0f;
        basis.forward = (Vector3){ 0.0f, sign, 0.0f };
        basis.right = (Vector3){ cy, 0.0f, -sy };
        basis.up = (Vector3){ -sign * sy, 0.0f, -sign * cy };
        return basis;
    }

    basis.forward = (Vector3){ cp * sy, sp, cp * cy };
    basis.right = (Vector3){ cy, 0.0f, -sy };
    basis.up = (Vector3){ -sp * sy, cp, -sp * cy };
    return basis;
}

void FpsLookSyncCamera(const FpsLook *look, Vector3 position, Camera3D *camera)
{
    FpsBasis basis = FpsLookGetBasis(look);

    camera->position = position;
    camera->target = Vector3Add(position, basis.forward);
    camera->up = basis.up;
}

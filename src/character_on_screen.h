#pragma once

#include <d3d9.h>

constexpr float kCharacterReach = 3.0f;
constexpr float kCharacterBelowFocus = 0.5f;
constexpr float kCharacterAboveFocus = 5.0f;
constexpr float kFarthestCharacterFocus = 100.0f;
constexpr float kCameraClearance = 0.25f;
constexpr float kNarrowestCharacterReach = 0.75f;

struct CharacterOnScreen
{
    bool present;
    float pixelMin[2];
    float pixelMax[2];
    float focusFromCamera[3];
    float reach;
};

CharacterOnScreen ProjectCharacterOntoViewport(const float* focus, const float* camPos,
                                               const float* cameraRelativeView, const float* glProjection,
                                               const D3DVIEWPORT9& vp);

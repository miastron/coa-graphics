#include "character_on_screen.h"

#include "fog_model.h"
#include "light_on_screen.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr float kNearestProjectedDepth = 0.25f;
constexpr int kBoxCorners = 8;
constexpr float kInfinity = std::numeric_limits<float>::infinity();

float ReachClearOfTheCamera(const float* toCamera)
{
    if (!(toCamera[2] > -kCharacterBelowFocus && toCamera[2] < kCharacterAboveFocus))
        return kCharacterReach;
    const float besideTheFocus = std::sqrt(toCamera[0] * toCamera[0] + toCamera[1] * toCamera[1]);
    return std::min(kCharacterReach, besideTheFocus - kCameraClearance);
}

void CornerFromCamera(const float* focus, const float* camPos, float reach, int corner, float* out)
{
    out[0] = focus[0] - camPos[0] + ((corner & 1) ? reach : -reach);
    out[1] = focus[1] - camPos[1] + ((corner & 2) ? reach : -reach);
    out[2] = focus[2] - camPos[2] + ((corner & 4) ? kCharacterAboveFocus : -kCharacterBelowFocus);
}

bool BoxCornersInFrontOfTheCamera(const float* focus, const float* camPos, float reach,
                                  const float* cameraRelativeView, const float* glProjection, const D3DVIEWPORT9& vp,
                                  float* pixelMin, float* pixelMax)
{
    pixelMin[0] = pixelMin[1] = kInfinity;
    pixelMax[0] = pixelMax[1] = -kInfinity;
    for (int corner = 0; corner < kBoxCorners; ++corner)
    {
        float fromCamera[3];
        CornerFromCamera(focus, camPos, reach, corner, fromCamera);
        float inView[3];
        TransformDirection(fromCamera, cameraRelativeView, inView);
        if (!(inView[2] >= kNearestProjectedDepth))
            return false;
        const LightOnScreen onScreen = ProjectLightOntoViewport(inView, glProjection, vp);
        for (int axis = 0; axis < 2; ++axis)
        {
            pixelMin[axis] = std::min(pixelMin[axis], onScreen.pixel[axis]);
            pixelMax[axis] = std::max(pixelMax[axis], onScreen.pixel[axis]);
        }
    }
    return true;
}
}

CharacterOnScreen ProjectCharacterOntoViewport(const float* focus, const float* camPos,
                                               const float* cameraRelativeView, const float* glProjection,
                                               const D3DVIEWPORT9& vp)
{
    CharacterOnScreen character = {};
    const float toCamera[3] = {camPos[0] - focus[0], camPos[1] - focus[1], camPos[2] - focus[2]};
    const float distance =
        std::sqrt(toCamera[0] * toCamera[0] + toCamera[1] * toCamera[1] + toCamera[2] * toCamera[2]);
    const float reach = ReachClearOfTheCamera(toCamera);
    if (!(distance <= kFarthestCharacterFocus) || !(reach >= kNarrowestCharacterReach))
        return character;
    for (int axis = 0; axis < 3; ++axis)
        character.focusFromCamera[axis] = -toCamera[axis];
    character.reach = reach;
    if (!BoxCornersInFrontOfTheCamera(focus, camPos, reach, cameraRelativeView, glProjection, vp,
                                      character.pixelMin, character.pixelMax))
    {
        character.pixelMin[0] = static_cast<float>(vp.X);
        character.pixelMin[1] = static_cast<float>(vp.Y);
        character.pixelMax[0] = static_cast<float>(vp.X + vp.Width);
        character.pixelMax[1] = static_cast<float>(vp.Y + vp.Height);
    }
    character.present = std::isfinite(character.pixelMin[0]) && std::isfinite(character.pixelMin[1]) &&
                        std::isfinite(character.pixelMax[0]) && std::isfinite(character.pixelMax[1]);
    return character;
}

#pragma once

#include "shadow_light.h"

namespace shadow_light_checks
{
constexpr float kTolerance = 1e-3f;
constexpr float kClientVector[3] = {-0.405f, -0.405f, -0.820f};
constexpr float kClientToLight[3] = {0.405f, 0.405f, 0.820f};
constexpr float kSkyToLight[3] = {0.703f, 0.703f, 0.110f};
constexpr float kShortVector[3] = {0.0f, 0.0f, -0.9f};
constexpr float kUpward[3] = {0.0f, 0.0f, 1.0f};

bool NearVector(const float* a, const float* b)
{
    for (int axis = 0; axis < 3; ++axis)
        if (std::fabs(a[axis] - b[axis]) > kTolerance)
            return false;
    return true;
}

void CheckShadowDirection()
{
    float toLight[3];

    ShadowDirection(kClientVector, kSkyToLight, toLight);
    Check(NearVector(toLight, kClientToLight),
          "with the native maps absent, the shadow trace follows the client's shadow light, not the sky body");

    ShadowDirection(kShortVector, kSkyToLight, toLight);
    Check(NearVector(toLight, kUpward), "a client vector of length 0.9 is normalised and negated toward the light");

    const float zeroVector[3] = {0.0f, 0.0f, 0.0f};
    ShadowDirection(zeroVector, kSkyToLight, toLight);
    Check(NearVector(toLight, kSkyToLight), "an unset client shadow light falls back to the sky body");

    const float tooLong[3] = {-1.6f, -1.6f, -1.6f};
    ShadowDirection(tooLong, kSkyToLight, toLight);
    Check(NearVector(toLight, kSkyToLight), "an out-of-range client shadow light falls back to the sky body");

    const float notANumber[3] = {NAN, -0.405f, -0.820f};
    ShadowDirection(notANumber, kSkyToLight, toLight);
    Check(NearVector(toLight, kSkyToLight), "a non-finite client shadow light falls back to the sky body");
}

void CheckShaftsFollowLight()
{
    Check(ShaftsFollowLight(kClientToLight, kClientToLight), "a light along the client shadow light keeps the shafts");
    Check(!ShaftsFollowLight(kSkyToLight, kClientToLight),
          "a low sun about 50 degrees from the client shadow light has no shafts");
    const float nearLight[3] = {0.5f, 0.405f, 0.77f};
    Check(ShaftsFollowLight(nearLight, kClientToLight), "a light a few degrees off keeps the shafts");
    const float twentyDegrees[3] = {0.3420f, 0.0f, 0.9397f};
    const float thirtyDegrees[3] = {0.5f, 0.0f, 0.8660f};
    Check(ShaftsFollowLight(twentyDegrees, kUpward) && !ShaftsFollowLight(thirtyDegrees, kUpward),
          "the shafts switch off between 20 and 30 degrees from the client shadow light");
}
}

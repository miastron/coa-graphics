#pragma once

#include <algorithm>
#include <cmath>

inline bool ShadowLightFromClientVector(const float* clientVector, float* toLight)
{
    float lengthSquared = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (!std::isfinite(clientVector[axis]))
            return false;
        lengthSquared += clientVector[axis] * clientVector[axis];
    }
    if (!(lengthSquared > 0.5f && lengthSquared < 1.5f))
        return false;
    const float inverseLength = -1.0f / std::sqrt(lengthSquared);
    for (int axis = 0; axis < 3; ++axis)
        toLight[axis] = clientVector[axis] * inverseLength;
    return true;
}

constexpr float kShaftsOffBeyondDegrees = 25.0f;

inline bool ShaftsFollowLight(const float* skyToLight, const float* shadowToLight)
{
    constexpr float kDegreesToRadians = 3.14159265f / 180.0f;
    const float dot = skyToLight[0] * shadowToLight[0] + skyToLight[1] * shadowToLight[1] +
                      skyToLight[2] * shadowToLight[2];
    return dot >= std::cos(kShaftsOffBeyondDegrees * kDegreesToRadians);
}

inline void ShadowDirection(const float* clientVector, const float* skyToLight, float* toLight)
{
    if (!ShadowLightFromClientVector(clientVector, toLight))
        std::copy_n(skyToLight, 3, toLight);
}

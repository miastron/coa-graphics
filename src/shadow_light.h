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

inline void ShadowDirection(const float* clientVector, const float* skyToLight, float* toLight)
{
    if (!ShadowLightFromClientVector(clientVector, toLight))
        std::copy_n(skyToLight, 3, toLight);
}

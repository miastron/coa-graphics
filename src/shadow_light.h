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

constexpr float kShadowMapsFullWithinDegrees = 25.0f;
constexpr float kShadowMapsGoneBeyondDegrees = 35.0f;

inline float ShadowMapWeight(const float* skyToLight, const float* shadowToLight)
{
    constexpr float kRadiansToDegrees = 180.0f / 3.14159265f;
    const float dot = skyToLight[0] * shadowToLight[0] + skyToLight[1] * shadowToLight[1] +
                      skyToLight[2] * shadowToLight[2];
    const float degrees = std::acos(std::clamp(dot, -1.0f, 1.0f)) * kRadiansToDegrees;
    const float t = std::clamp((degrees - kShadowMapsFullWithinDegrees) /
                                   (kShadowMapsGoneBeyondDegrees - kShadowMapsFullWithinDegrees),
                               0.0f, 1.0f);
    return 1.0f - t * t * (3.0f - 2.0f * t);
}

inline void ShadowDirection(const float* clientVector, const float* skyToLight, float* toLight)
{
    if (!ShadowLightFromClientVector(clientVector, toLight))
        std::copy_n(skyToLight, 3, toLight);
}

struct SunShadowPlan
{
    float toLight[3];
    float mapWeight;
};

inline SunShadowPlan PlanSunShadows(const float* clientVector, const float* skyToLight)
{
    SunShadowPlan plan = {};
    float clientToLight[3];
    if (!ShadowLightFromClientVector(clientVector, clientToLight))
    {
        std::copy_n(skyToLight, 3, plan.toLight);
        plan.mapWeight = 1.0f;
        return plan;
    }
    plan.mapWeight = ShadowMapWeight(skyToLight, clientToLight);
    float length = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
    {
        plan.toLight[axis] = plan.mapWeight * clientToLight[axis] + (1.0f - plan.mapWeight) * skyToLight[axis];
        length += plan.toLight[axis] * plan.toLight[axis];
    }
    if (!(length > 1e-6f))
    {
        std::copy_n(skyToLight, 3, plan.toLight);
        return plan;
    }
    const float inverseLength = 1.0f / std::sqrt(length);
    for (float& axis : plan.toLight)
        axis *= inverseLength;
    return plan;
}

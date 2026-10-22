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
constexpr float kUpwardVector[3] = {0.0f, 0.0f, -1.0f};

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

void CheckSunShadowPlan()
{
    const SunShadowPlan overhead = PlanSunShadows(kClientVector, kClientToLight);
    Check(overhead.mapWeight == 1.0f && NearVector(overhead.toLight, kClientToLight),
          "a light along the client shadow light traces along it with the native shadow maps at full weight");

    const SunShadowPlan lowSun = PlanSunShadows(kClientVector, kSkyToLight);
    Check(lowSun.mapWeight == 0.0f && NearVector(lowSun.toLight, kSkyToLight),
          "a low sun far from the client shadow light is traced along the sun itself, without the native shadow maps");

    const float zeroVector[3] = {0.0f, 0.0f, 0.0f};
    const SunShadowPlan unset = PlanSunShadows(zeroVector, kSkyToLight);
    Check(unset.mapWeight == 1.0f && NearVector(unset.toLight, kSkyToLight),
          "an unset client shadow light traces along the sky body with the maps, as before");
}

void CheckShadowMapWeightFades()
{
    const float twentyDegrees[3] = {0.3420f, 0.0f, 0.9397f};
    const float thirtyDegrees[3] = {0.5f, 0.0f, 0.8660f};
    const float fortyDegrees[3] = {0.6428f, 0.0f, 0.7660f};
    const float within = ShadowMapWeight(twentyDegrees, kUpward);
    const float middle = ShadowMapWeight(thirtyDegrees, kUpward);
    const float beyond = ShadowMapWeight(fortyDegrees, kUpward);
    Check(within == 1.0f && beyond == 0.0f && middle > 0.4f && middle < 0.6f,
          "the native shadow maps keep full weight to 25 degrees from the client shadow light, half at 30 and none "
          "from 35");
    float previous = 1.0f;
    bool falling = true;
    for (int tenths = 250; tenths <= 350; ++tenths)
    {
        const float radians = static_cast<float>(tenths) * 0.1f * 3.14159265f / 180.0f;
        const float light[3] = {std::sin(radians), 0.0f, std::cos(radians)};
        const float weight = ShadowMapWeight(light, kUpward);
        falling = falling && weight <= previous + 1e-6f;
        previous = weight;
    }
    Check(falling, "the native shadow map weight falls steadily between 25 and 35 degrees, so the shafts fade rather "
                   "than pop");
    const float blended[3] = {0.5f, 0.0f, 0.8660f};
    const SunShadowPlan plan = PlanSunShadows(kUpwardVector, blended);
    float dot = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
        dot += plan.toLight[axis] * blended[axis];
    Check(plan.mapWeight > 0.0f && plan.mapWeight < 1.0f && dot > 0.9f && dot < 1.0f - 1e-4f,
          "between 25 and 35 degrees the trace direction turns from the client shadow light toward the sun");
}
}

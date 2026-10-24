#pragma once

#include <cmath>

namespace god_ray_blur
{
constexpr int kPasses = 2;
constexpr int kTaps = 8;
constexpr float kFirstPassStep = 0.075f;
constexpr float kDecayPerFirstPassTap = 0.9f;

struct Pass
{
    float stepTowardSun;
    float decayPerTap;
    float normalisation;
};

inline Pass PassAt(int pass)
{
    float tapsPerFirstPassTap = 1.0f;
    for (int i = 0; i < pass; ++i)
        tapsPerFirstPassTap *= kTaps;
    Pass result = {};
    result.stepTowardSun = kFirstPassStep / tapsPerFirstPassTap;
    result.decayPerTap = std::pow(kDecayPerFirstPassTap, 1.0f / tapsPerFirstPassTap);
    float sum = 0.0f;
    for (int tap = 0; tap < kTaps; ++tap)
        sum += std::pow(result.decayPerTap, static_cast<float>(tap));
    result.normalisation = 1.0f / sum;
    return result;
}

inline float TapWeight(int pass, int tap)
{
    return std::pow(PassAt(pass).decayPerTap, static_cast<float>(tap)) * PassAt(pass).normalisation;
}
}

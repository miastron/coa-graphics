#include "grass_density.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
constexpr float kGoldenAngle = 2.3999632f;
constexpr float kGoldenFraction = 0.6180339f;
constexpr float kShortestCopyShare = 0.5f;
constexpr float kRoundingHalf = 0.5f;
constexpr float kCopyCountEpsilon = 1.0e-4f;
constexpr float kTwoPi = 6.2831853f;
constexpr float kPi = 3.1415927f;
constexpr uint32_t kHashSecondMultiplier = 0x9E3779B1u;
constexpr uint32_t kHashFirstFinal = 0x85EBCA6Bu;
constexpr uint32_t kHashSecondFinal = 0xC2B2AE35u;
constexpr unsigned kHashShortShift = 13;
constexpr unsigned kHashLongShift = 16;
}

GrassDensitySettings GrassDensitySettingsFrom(const Config& c)
{
    return {c.grassDensity, c.grassDensityRadius, c.grassDensitySpread, c.grassDensityFade};
}

float GrassChunkDistance(const float* worldMatrix)
{
    const float half = kGrassChunkSize * 0.5f;
    const float centreX = worldMatrix[kWorldMatrixTranslationX] - half;
    const float centreY = worldMatrix[kWorldMatrixTranslationY] - half;
    const float outsideX = std::max(std::fabs(centreX) - half, 0.0f);
    const float outsideY = std::max(std::fabs(centreY) - half, 0.0f);
    return std::sqrt(outsideX * outsideX + outsideY * outsideY);
}

unsigned GrassCopyCount(const GrassDensitySettings& settings)
{
    const float extra = settings.multiplier - 1.0f;
    if (!(extra > kCopyCountEpsilon) || !(settings.radius > 0.0f))
        return 0;
    return std::min(static_cast<unsigned>(std::ceil(extra - kCopyCountEpsilon)), kGrassMaxExtraCopies);
}

GrassCopyWindow GrassCopyWindowFor(const GrassDensitySettings& settings, unsigned copy)
{
    const float extra = std::max(settings.multiplier - 1.0f, kCopyCountEpsilon);
    const float band = settings.radius * (1.0f - kGrassFullDensityShare);
    const float before = static_cast<float>(copy > 0 ? copy - 1 : 0);
    const float share = std::clamp(extra - before, kCopyCountEpsilon, 1.0f);
    return {settings.radius - band * before / extra, band * share / extra, 1.0f / share,
            1.0f / std::max(settings.fade, kCopyCountEpsilon)};
}

unsigned GrassExtraCopies(const GrassDensitySettings& settings, float chunkDistance)
{
    const unsigned total = GrassCopyCount(settings);
    unsigned copies = 0;
    for (unsigned copy = 1; copy <= total; ++copy)
        if (chunkDistance < GrassCopyWindowFor(settings, copy).outerDistance)
            ++copies;
    return copies;
}

float GrassPlantSink(const GrassCopyWindow& window, unsigned plantBits, float distance)
{
    if (window.width < kGrassWindowMinimum)
        return 0.0f;
    const float unit = (static_cast<float>(plantBits) + kRoundingHalf) / static_cast<float>(1u << kGrassPlantBitCount);
    const float steps = unit * kGrassThresholdMix;
    const float threshold = (steps - std::floor(steps)) * window.inverseShare;
    const float hideDistance = window.outerDistance - threshold * window.width;
    const float ramp = std::clamp(1.0f - (hideDistance - distance) * window.inverseFade, 0.0f, 1.0f);
    return (threshold >= 1.0f ? 1.0f : ramp) * kGrassSinkDepth;
}

unsigned GrassPlantBits(float x, float y)
{
    uint32_t bitsX;
    uint32_t bitsY;
    std::memcpy(&bitsX, &x, sizeof(bitsX));
    std::memcpy(&bitsY, &y, sizeof(bitsY));
    uint32_t hash = bitsX ^ (bitsY * kHashSecondMultiplier);
    hash ^= hash >> kHashLongShift;
    hash *= kHashFirstFinal;
    hash ^= hash >> kHashShortShift;
    hash *= kHashSecondFinal;
    hash ^= hash >> kHashLongShift;
    return hash >> (sizeof(hash) * 8 - kGrassPlantBitCount);
}

GrassCopyOffset GrassPlantOffset(const GrassCopyOffset& copy, unsigned plantBits)
{
    const float unit = (static_cast<float>(plantBits) + kRoundingHalf) / static_cast<float>(1u << kGrassPlantBitCount);
    const float angle = unit * kTwoPi - kPi;
    const float steps = unit * kGrassPlantMagnitudeMix;
    const float magnitude = kGrassPlantMagnitudeFloor + kGrassPlantMagnitudeRange * (steps - std::floor(steps));
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return {(copy.x * cosine - copy.y * sine) * magnitude, (copy.x * sine + copy.y * cosine) * magnitude};
}

GrassCopyOffset GrassCopyOffsetFor(const GrassDensitySettings& settings, unsigned copy)
{
    const float angle = static_cast<float>(copy) * kGoldenAngle;
    const float steps = static_cast<float>(copy) * kGoldenFraction;
    const float share = kShortestCopyShare + (1.0f - kShortestCopyShare) * (steps - std::floor(steps));
    const float distance = settings.spread * share;
    return {distance * std::cos(angle), distance * std::sin(angle)};
}

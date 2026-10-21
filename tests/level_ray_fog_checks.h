#pragma once

namespace level_ray_fog
{
constexpr float kSlabDensity = 0.01f;
constexpr float kSlabEmissive = 0.5f;
constexpr float kSlabDistance = 100.0f;
constexpr float kFarthestDistance = 99999.0f;
constexpr float kMaxDistance = 1000.0f;
constexpr float kTolerance = 1e-5f;

FogParams SlabParams(float emissive)
{
    FogParams fog = {};
    fog.maxDistance = kMaxDistance;
    FogLayer& layer = fog.layers[0];
    layer.density = kSlabDensity;
    layer.exponent = 1.0f;
    layer.shadowDensity = 1.0f;
    layer.endDistance = kFarthestDistance;
    for (int c = 0; c < 3; ++c)
        layer.emissive[c] = emissive;
    return fog;
}

void CheckLevelRayFog()
{
    const FogParams fog = SlabParams(kSlabEmissive);
    const float expectedOpacity = 1.0f - std::exp(-kSlabDensity * kSlabDistance);
    const LevelRayFog slab = LevelRayFogAt(fog, kSlabDistance);
    const LevelRayFog none = LevelRayFogAt(SlabParams(0.0f), kSlabDistance);
    const LevelRayFog doubled = LevelRayFogAt(SlabParams(2.0f * kSlabEmissive), kSlabDistance);
    std::printf("     uniform slab at %.0f yd: opacity %.4f (expected %.4f), glow %.4f, doubled emission glow %.4f\n",
                kSlabDistance, slab.opacity, expectedOpacity, slab.emission, doubled.emission);
    Check(std::fabs(slab.opacity - expectedOpacity) < kTolerance &&
              std::fabs(slab.emission - kSlabEmissive * expectedOpacity) < kTolerance,
          "a uniform slab's level-ray opacity is 1-exp(-density*distance) and its glow is the emission times it");
    Check(none.emission == 0.0f && std::fabs(doubled.emission - 2.0f * slab.emission) < kTolerance,
          "the level-ray glow scales with the layers' emission");
    Check(LevelRayFogAt(fog, 2.0f * kSlabDistance).opacity > slab.opacity &&
              LevelRayFogAt(FogParams{}, kSlabDistance).opacity == 0.0f,
          "the level-ray opacity grows with distance and an empty fog has none");
}
}

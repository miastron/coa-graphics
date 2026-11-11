#pragma once

namespace surface_light_area_checks
{
constexpr UINT kViewWidth = 1920;
constexpr UINT kViewHeight = 1200;
constexpr D3DVIEWPORT9 kView = {0, 0, kViewWidth, kViewHeight, 0.0f, 1.0f};
constexpr float kElwynnLampColour[3] = {1.5f, 0.4026f, 0.04349f};
constexpr float kElwynnLampAttenuation[3] = {0.0f, 0.7f, 0.03f};
constexpr float kElwynnLampCutoff = 46.1f;
constexpr float kFarAhead = 150.0f;
constexpr float kInsideTheSphere = 30.0f;
constexpr float kBarelyOn = 0.003f;
constexpr float kInvisiblyFaint = 0.00001f;
constexpr LONG kPixelSlack = 3;
constexpr float kFadeSharpness = 4.0f;
constexpr int kSampledPoints = 200000;
constexpr float kSampledDepth = 260.0f;
constexpr float kSampledNearest = 0.2f;

SurfaceLight Lamp(float across, float up, float ahead, float strength = 1.0f)
{
    SurfaceLight lamp = {};
    lamp.viewPosition[0] = across;
    lamp.viewPosition[1] = up;
    lamp.viewPosition[2] = ahead;
    lamp.cutoff = kElwynnLampCutoff;
    lamp.strength = strength;
    for (int i = 0; i < 3; ++i)
    {
        lamp.colour[i] = kElwynnLampColour[i];
        lamp.attenuation[i] = kElwynnLampAttenuation[i];
    }
    return lamp;
}

SurfaceLightArea AreaOf(const std::vector<SurfaceLight>& lamps, const float* projection)
{
    return ProjectSurfaceLights(lamps.data(), static_cast<uint32_t>(lamps.size()), projection, kView);
}

bool WholeView(const SurfaceLightArea& area)
{
    return area.inViewport.left == 0 && area.inViewport.top == 0 &&
           area.inViewport.right == static_cast<LONG>(kViewWidth) &&
           area.inViewport.bottom == static_cast<LONG>(kViewHeight) && area.share == 1.0f;
}

float LightOnASurfaceFacingIt(const SurfaceLight& lamp, const float point[3])
{
    const float toLight[3] = {lamp.viewPosition[0] - point[0], lamp.viewPosition[1] - point[1],
                              lamp.viewPosition[2] - point[2]};
    const float distanceSquared =
        std::max(toLight[0] * toLight[0] + toLight[1] * toLight[1] + toLight[2] * toLight[2], 1e-6f);
    if (distanceSquared >= lamp.cutoff * lamp.cutoff)
        return 0.0f;
    const float distance = std::sqrt(distanceSquared);
    const float falloff = 1.0f / std::max(lamp.attenuation[0] + lamp.attenuation[1] * distance +
                                              lamp.attenuation[2] * distanceSquared,
                                          1.0f);
    const float fade = std::clamp((lamp.cutoff - distance) * kFadeSharpness / std::max(lamp.cutoff, 1e-3f), 0.0f, 1.0f);
    return std::max({lamp.colour[0], lamp.colour[1], lamp.colour[2]}) * lamp.strength * falloff * fade;
}

struct SampledPoints
{
    int lit = 0;
    int litOutsideTheArea = 0;
    int unlitOutsideTheArea = 0;
};

SampledPoints SamplePointsInView(const std::vector<SurfaceLight>& lamps, const float* projection)
{
    const SurfaceLightArea area = AreaOf(lamps, projection);
    SampledPoints sampled;
    uint32_t state = 0x2545F491u;
    auto unit = [&state] {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) / 16777216.0f;
    };
    for (int i = 0; i < kSampledPoints; ++i)
    {
        const float pixelX = unit() * kViewWidth;
        const float pixelY = unit() * kViewHeight;
        const float depth = kSampledNearest + unit() * unit() * kSampledDepth;
        const float ndcX = pixelX / kViewWidth * 2.0f - 1.0f;
        const float ndcY = 1.0f - pixelY / kViewHeight * 2.0f;
        const float point[3] = {(ndcX - projection[8]) / projection[0] * depth,
                                (ndcY - projection[9]) / projection[5] * depth, depth};
        float light = 0.0f;
        for (const SurfaceLight& lamp : lamps)
            light += LightOnASurfaceFacingIt(lamp, point);
        const LONG column = static_cast<LONG>(pixelX);
        const LONG row = static_cast<LONG>(pixelY);
        const bool inside = column >= area.inViewport.left && column < area.inViewport.right &&
                            row >= area.inViewport.top && row < area.inViewport.bottom;
        const bool lit = light >= kFaintestSurfaceLight;
        sampled.lit += lit ? 1 : 0;
        sampled.litOutsideTheArea += lit && !inside ? 1 : 0;
        sampled.unlitOutsideTheArea += !lit && !inside ? 1 : 0;
    }
    return sampled;
}

void CheckLampSpheresProjectOntoTheView()
{
    float projection[16];
    EngineProjection(static_cast<float>(kViewWidth) / kViewHeight, projection);
    const SurfaceLightArea ahead = AreaOf({Lamp(0, 0, kFarAhead)}, projection);
    const float halfWidth = std::tan(std::asin(kElwynnLampCutoff / kFarAhead)) * projection[0] * kViewWidth * 0.5f;
    const float halfHeight = std::tan(std::asin(kElwynnLampCutoff / kFarAhead)) * projection[5] * kViewHeight * 0.5f;
    const LONG expectedLeft = static_cast<LONG>(kViewWidth * 0.5f - halfWidth);
    const LONG expectedTop = static_cast<LONG>(kViewHeight * 0.5f - halfHeight);
    std::printf("     an Elwynn lamp (reach %.1f yd) %.0f yd ahead in a %ux%u view: rectangle %ld,%ld to %ld,%ld, "
                "%.1f%% of the view\n",
                kElwynnLampCutoff, kFarAhead, kViewWidth, kViewHeight, ahead.inViewport.left, ahead.inViewport.top,
                ahead.inViewport.right, ahead.inViewport.bottom, 100.0f * ahead.share);
    Check(std::abs(ahead.inViewport.left - expectedLeft) <= kPixelSlack &&
              std::abs(ahead.inViewport.top - expectedTop) <= kPixelSlack &&
              std::abs(ahead.inViewport.right - (static_cast<LONG>(kViewWidth) - expectedLeft)) <= kPixelSlack &&
              std::abs(ahead.inViewport.bottom - (static_cast<LONG>(kViewHeight) - expectedTop)) <= kPixelSlack &&
              ahead.share > 0.0f && ahead.share < 1.0f,
          "a lamp ahead of the camera lights surfaces inside the rectangle its sphere's tangents project to");
    Check(WholeView(AreaOf({Lamp(0, 0, kInsideTheSphere)}, projection)) &&
              WholeView(AreaOf({Lamp(0, 0, -kInsideTheSphere)}, projection)),
          "a lamp whose sphere holds the camera, ahead of it or behind, may light the whole view");
    Check(AreaOf({Lamp(0, 0, -kFarAhead)}, projection).Empty() && AreaOf({}, projection).Empty(),
          "a lamp whose sphere lies behind the camera lights nothing in view, and neither do no lamps");
    const SurfaceLightArea besideTheCamera = AreaOf({Lamp(60, 0, 20)}, projection);
    Check(besideTheCamera.inViewport.right == static_cast<LONG>(kViewWidth) &&
              besideTheCamera.inViewport.left > static_cast<LONG>(kViewWidth / 2) &&
              besideTheCamera.inViewport.top == 0 &&
              besideTheCamera.inViewport.bottom == static_cast<LONG>(kViewHeight),
          "a sphere that crosses the camera's plane to the right reaches the view's right edge and its full height, "
          "and leaves the left half alone");
    const SurfaceLightArea apart = AreaOf({Lamp(-90, 0, kFarAhead), Lamp(90, 0, kFarAhead)}, projection);
    std::printf("     two lamps 180 yd apart: one rectangle around both %.1f%% of the view, their own rectangles "
                "%.1f%%\n",
                100.0f * apart.share, 100.0f * apart.shareOfLightsApart);
    Check(apart.share > apart.shareOfLightsApart && apart.shareOfLightsApart > 0.0f,
          "lamps far apart share one rectangle around both, and the area reports how much less their own "
          "rectangles cover");
    const SurfaceLight shipped = Lamp(0, 0, kFarAhead);
    const SurfaceLight barelyOn = Lamp(0, 0, kFarAhead, kBarelyOn);
    const float aloneAndBarelyOn = SurfaceLightRadius(barelyOn, 1);
    std::printf("     surface reach of the lamp: %.1f yd at strength 1, %.1f yd alone at %.3f, %.1f yd among eight "
                "at %.3f\n",
                SurfaceLightRadius(shipped, kMaxLocalPointLights), aloneAndBarelyOn, kBarelyOn,
                SurfaceLightRadius(barelyOn, kMaxLocalPointLights), kBarelyOn);
    Check(SurfaceLightRadius(shipped, kMaxLocalPointLights) == kElwynnLampCutoff && aloneAndBarelyOn > 0.0f &&
              aloneAndBarelyOn < kElwynnLampCutoff &&
              SurfaceLightRadius(barelyOn, kMaxLocalPointLights) > aloneAndBarelyOn,
          "a lamp reaches surfaces as far as its sphere at the shipped strength, less far when its strength leaves "
          "it under the shader's faintest light sooner, and farther among lamps that may add up");
    Check(AreaOf({Lamp(0, 0, kInsideTheSphere, kInvisiblyFaint)}, projection).Empty() &&
              AreaOf({Lamp(0, 0, kInsideTheSphere, 0.0f)}, projection).Empty(),
          "a strength too low to reach the shader's faintest light anywhere, or none, lights nothing");
}

void CheckNoLitPointLiesOutsideTheArea()
{
    float projection[16];
    EngineProjection(static_cast<float>(kViewWidth) / kViewHeight, projection);
    struct Case
    {
        const char* name;
        std::vector<SurfaceLight> lamps;
    };
    const Case cases[] = {
        {"three lamps far ahead", {Lamp(-70, 5, 180), Lamp(20, -10, 120), Lamp(95, 0, 210)}},
        {"a lamp beside the camera", {Lamp(60, 0, 20)}},
        {"eight barely lit lamps along a street to the right",
         {Lamp(30, -3, 120, kBarelyOn), Lamp(42, -3, 128, kBarelyOn), Lamp(55, -2, 140, kBarelyOn),
          Lamp(66, -4, 150, kBarelyOn), Lamp(78, -3, 164, kBarelyOn), Lamp(90, -3, 176, kBarelyOn),
          Lamp(101, -2, 190, kBarelyOn), Lamp(5, -3, -60, kBarelyOn)}},
    };
    for (const Case& sampledCase : cases)
    {
        const SampledPoints sampled = SamplePointsInView(sampledCase.lamps, projection);
        char label[200];
        std::snprintf(label, sizeof(label),
                      "%s: of %d points in view %d take light, none of them outside the rectangle (%d), which "
                      "leaves %d unlit points out",
                      sampledCase.name, kSampledPoints, sampled.lit, sampled.litOutsideTheArea,
                      sampled.unlitOutsideTheArea);
        Check(sampled.lit > 0 && sampled.litOutsideTheArea == 0 && sampled.unlitOutsideTheArea > 0, label);
    }
}
}

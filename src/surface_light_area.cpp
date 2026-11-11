#include "surface_light_area.h"

#include "engine_lights.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kHalfTurn = 3.14159265358979323846;
constexpr double kQuarterTurn = kHalfTurn / 2.0;
constexpr double kSteepestTangent = 1e6;
constexpr double kFaintnessMargin = 0.95;
constexpr double kPixelMargin = 1.0;

struct TangentRange
{
    bool inFront;
    double low;
    double high;
};

struct PixelRange
{
    LONG first;
    LONG end;
};

double PeakColour(const SurfaceLight& light)
{
    return std::max({light.colour[0], light.colour[1], light.colour[2]});
}

TangentRange TangentsInFront(double across, double ahead, double radius)
{
    const double distanceSquared = across * across + ahead * ahead;
    if (distanceSquared <= radius * radius)
        return {true, -kSteepestTangent, kSteepestTangent};
    const double centre = std::atan2(across, ahead);
    const double halfWidth = std::asin(radius / std::sqrt(distanceSquared));
    for (int turn = -1; turn <= 1; ++turn)
    {
        const double low = centre - halfWidth + turn * 2.0 * kHalfTurn;
        const double high = centre + halfWidth + turn * 2.0 * kHalfTurn;
        if (high <= -kQuarterTurn || low >= kQuarterTurn)
            continue;
        return {true, low <= -kQuarterTurn ? -kSteepestTangent : std::max(std::tan(low), -kSteepestTangent),
                high >= kQuarterTurn ? kSteepestTangent : std::min(std::tan(high), kSteepestTangent)};
    }
    return {false, 0.0, 0.0};
}

double PixelAt(double tangent, double ndcScale, double ndcOffset, double size, bool downward)
{
    const double ndc = tangent * ndcScale + ndcOffset;
    return (downward ? 0.5 - ndc * 0.5 : ndc * 0.5 + 0.5) * size;
}

PixelRange PixelsCovered(const TangentRange& tangents, double ndcScale, double ndcOffset, DWORD size, bool downward)
{
    const double one = PixelAt(tangents.low, ndcScale, ndcOffset, size, downward);
    const double other = PixelAt(tangents.high, ndcScale, ndcOffset, size, downward);
    const double last = static_cast<double>(size);
    const double first = std::clamp(std::floor(std::min(one, other)) - kPixelMargin, 0.0, last);
    const double end = std::clamp(std::ceil(std::max(one, other)) + kPixelMargin, 0.0, last);
    return {static_cast<LONG>(first), static_cast<LONG>(end)};
}

bool SphereOnScreen(const float centre[3], float radius, const float* glProjection, const D3DVIEWPORT9& vp,
                    RECT& rect)
{
    const TangentRange across = TangentsInFront(centre[0], centre[2], radius);
    const TangentRange upward = TangentsInFront(centre[1], centre[2], radius);
    if (!across.inFront || !upward.inFront)
        return false;
    const PixelRange columns = PixelsCovered(across, glProjection[0], glProjection[8], vp.Width, false);
    const PixelRange rows = PixelsCovered(upward, glProjection[5], glProjection[9], vp.Height, true);
    rect = {columns.first, rows.first, columns.end, rows.end};
    return columns.end > columns.first && rows.end > rows.first;
}

double Pixels(const RECT& rect)
{
    return static_cast<double>(rect.right - rect.left) * static_cast<double>(rect.bottom - rect.top);
}

RECT Around(const RECT& one, const RECT& other)
{
    return {std::min(one.left, other.left), std::min(one.top, other.top), std::max(one.right, other.right),
            std::max(one.bottom, other.bottom)};
}

uint32_t LightsOnSurfaces(const SurfaceLight* lights, uint32_t count)
{
    uint32_t onSurfaces = 0;
    for (uint32_t i = 0; i < count; ++i)
        if (lights[i].strength > 0.0f && PeakColour(lights[i]) > 0.0)
            ++onSurfaces;
    return onSurfaces;
}
}

float SurfaceLightRadius(const SurfaceLight& light, uint32_t lightsOnSurfaces)
{
    const double peak = PeakColour(light) * light.strength;
    if (!(peak > 0.0) || lightsOnSurfaces == 0)
        return 0.0f;
    const double tooFaintAlone = kFaintestSurfaceLight * kFaintnessMargin / lightsOnSurfaces;
    return std::min(light.cutoff, engine::DistanceWhereAttenuationReaches(light.attenuation, peak / tooFaintAlone));
}

SurfaceLightArea ProjectSurfaceLights(const SurfaceLight* lights, uint32_t count, const float* glProjection,
                                      const D3DVIEWPORT9& vp)
{
    SurfaceLightArea area;
    if (vp.Width == 0 || vp.Height == 0)
        return area;
    const double viewport = static_cast<double>(vp.Width) * vp.Height;
    const uint32_t onSurfaces = LightsOnSurfaces(lights, count);
    double apart = 0.0;
    bool any = false;
    for (uint32_t i = 0; i < count; ++i)
    {
        const float radius = SurfaceLightRadius(lights[i], onSurfaces);
        RECT rect = {};
        if (!(radius > 0.0f) || !SphereOnScreen(lights[i].viewPosition, radius, glProjection, vp, rect))
            continue;
        apart += Pixels(rect);
        area.inViewport = any ? Around(area.inViewport, rect) : rect;
        any = true;
    }
    if (!any)
        return area;
    area.share = static_cast<float>(Pixels(area.inViewport) / viewport);
    area.shareOfLightsApart = static_cast<float>(apart / viewport);
    return area;
}

SurfaceLightArea WholeViewport(const D3DVIEWPORT9& vp)
{
    SurfaceLightArea area;
    area.inViewport = {0, 0, static_cast<LONG>(vp.Width), static_cast<LONG>(vp.Height)};
    area.share = vp.Width > 0 && vp.Height > 0 ? 1.0f : 0.0f;
    area.shareOfLightsApart = area.share;
    return area;
}

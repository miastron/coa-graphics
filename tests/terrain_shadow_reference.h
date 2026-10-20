#pragma once

namespace terrain_shadow_reference
{
inline float UpstreamShadowHeight(const float* shadowHeights, int line, int majorStride, int minorStride, float minor)
{
    const float cell = std::floor(minor);
    const int first = static_cast<int>(cell);
    const float fraction = minor - cell;
    if (first < 0 || first >= kTerrainWindowVertices)
        return kNoTerrainHeight;
    const float lower = shadowHeights[line * majorStride + first * minorStride];
    if (first + 1 >= kTerrainWindowVertices)
        return fraction > 0.0f ? kNoTerrainHeight : lower;
    const float upper = shadowHeights[line * majorStride + (first + 1) * minorStride];
    return lower + (upper - lower) * fraction;
}

inline void ShadowHeights(const float* heights, const float* toLight, float* shadowHeights)
{
    const float towardRows = -toLight[0];
    const float towardCols = -toLight[1];
    const float horizontal = std::sqrt(towardRows * towardRows + towardCols * towardCols);
    if (horizontal < 1e-6f)
    {
        std::memcpy(shadowHeights, heights, sizeof(float) * kTerrainWindowVertices * kTerrainWindowVertices);
        return;
    }
    const bool rowsMajor = std::fabs(towardRows) >= std::fabs(towardCols);
    const float major = rowsMajor ? towardRows : towardCols;
    const float slope = (rowsMajor ? towardCols : towardRows) / std::fabs(major);
    const int majorStride = rowsMajor ? kTerrainWindowVertices : 1;
    const int minorStride = rowsMajor ? 1 : kTerrainWindowVertices;
    const int step = major > 0.0f ? 1 : -1;
    const float drop = kTerrainVertexYards * std::sqrt(1.0f + slope * slope) * toLight[2] / horizontal;
    for (int k = 0; k < kTerrainWindowVertices; ++k)
    {
        const int line = step > 0 ? kTerrainWindowVertices - 1 - k : k;
        const int upstream = line + step;
        const bool hasUpstream = upstream >= 0 && upstream < kTerrainWindowVertices;
        for (int n = 0; n < kTerrainWindowVertices; ++n)
        {
            const int index = line * majorStride + n * minorStride;
            float height = heights[index];
            if (hasUpstream)
                height = std::max(height, UpstreamShadowHeight(shadowHeights, upstream, majorStride, minorStride,
                                                               n + slope) -
                                              drop);
            shadowHeights[index] = height;
        }
    }
}

inline void ExactShadowHeights(const float* heights, const float* toLight, float* shadowHeights)
{
    constexpr int side = kTerrainWindowVertices;
    const double towardRows = -static_cast<double>(toLight[0]);
    const double towardCols = -static_cast<double>(toLight[1]);
    const double horizontal = std::sqrt(towardRows * towardRows + towardCols * towardCols);
    std::vector<double> shadow(heights, heights + side * side);
    if (horizontal >= 1e-6)
    {
        const bool rowsMajor = std::fabs(towardRows) >= std::fabs(towardCols);
        const double major = rowsMajor ? towardRows : towardCols;
        const double slope = (rowsMajor ? towardCols : towardRows) / std::fabs(major);
        const int majorStride = rowsMajor ? side : 1;
        const int minorStride = rowsMajor ? 1 : side;
        const int step = major > 0.0 ? 1 : -1;
        const double drop = kTerrainVertexYards * std::sqrt(1.0 + slope * slope) * toLight[2] / horizontal;
        for (int k = 0; k < side; ++k)
        {
            const int line = step > 0 ? side - 1 - k : k;
            const int upstream = line + step;
            if (upstream < 0 || upstream >= side)
                continue;
            for (int n = 0; n < side; ++n)
            {
                const double minor = n + slope;
                const double cell = std::floor(minor);
                const int first = static_cast<int>(cell);
                const double fraction = minor - cell;
                double upstreamHeight = kNoTerrainHeight;
                if (first >= 0 && first < side)
                {
                    const double lower = shadow[upstream * majorStride + first * minorStride];
                    if (first + 1 < side)
                        upstreamHeight =
                            lower + (shadow[upstream * majorStride + (first + 1) * minorStride] - lower) * fraction;
                    else if (fraction <= 0.0)
                        upstreamHeight = lower;
                }
                const int index = line * majorStride + n * minorStride;
                shadow[index] = std::max(static_cast<double>(heights[index]), upstreamHeight - drop);
            }
        }
    }
    for (int i = 0; i < side * side; ++i)
        shadowHeights[i] = static_cast<float>(shadow[i]);
}
}

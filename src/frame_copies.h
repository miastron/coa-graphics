#pragma once

#include <cstddef>

enum class ColourCopySite
{
    WaterBegin,
    SurfaceLights,
    GodRaySource,
    FogComposite,
    LateGodRays,
    Grading,
    Image,
    Count,
};

enum class DepthRefreshSite
{
    WaterBegin,
    WaterEnd,
    Fog,
    Count,
};

constexpr int kColourCopySites = static_cast<int>(ColourCopySite::Count);
constexpr int kDepthRefreshSites = static_cast<int>(DepthRefreshSite::Count);

struct FrameCopyCounts
{
    unsigned frames = 0;
    unsigned colourCopies[kColourCopySites] = {};
    unsigned depthRefreshes[kDepthRefreshSites] = {};
    unsigned depthRefreshesUnwritten[kDepthRefreshSites] = {};
    unsigned depthCopies[kDepthRefreshSites] = {};
};

class FrameCopyCounters
{
public:
    void CountFrame() { ++m_counts.frames; }
    void CountColourCopy(ColourCopySite site) { ++m_counts.colourCopies[static_cast<int>(site)]; }
    void CountDepthRefresh(DepthRefreshSite site, bool depthWrittenSinceLastRefresh, bool copied);
    const FrameCopyCounts& Counts() const { return m_counts; }
    void Reset() { m_counts = {}; }

private:
    FrameCopyCounts m_counts;
};

const char* ColourCopySiteName(ColourCopySite site);
const char* DepthRefreshSiteName(DepthRefreshSite site);
void DescribeFrameCopies(const FrameCopyCounts& counts, char* text, size_t size);
FrameCopyCounters& GlobalFrameCopies();
void LogFrameCopiesWhenDue();

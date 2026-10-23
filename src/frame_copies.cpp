#include "frame_copies.h"

#include "log.h"

#include <windows.h>

#include <cstdio>

namespace
{
constexpr ULONGLONG kSummaryIntervalMs = 60000;
constexpr size_t kSummaryTextSize = 768;

constexpr const char* kColourCopySiteNames[kColourCopySites] = {
    "water begin", "surface lights", "god-ray source", "fog composite", "late god rays", "grading", "image",
};

constexpr const char* kDepthRefreshSiteNames[kDepthRefreshSites] = {
    "water begin",
    "water end",
    "fog",
};

ULONGLONG g_summaryStartMs = 0;

double PerFrame(unsigned count, unsigned frames)
{
    return frames ? static_cast<double>(count) / frames : 0.0;
}

template <typename... Values>
void Append(char* text, size_t size, size_t& used, const char* format, Values... values)
{
    if (used >= size)
        return;
    const int written = std::snprintf(text + used, size - used, format, values...);
    if (written > 0)
        used += static_cast<size_t>(written);
}
}

void FrameCopyCounters::CountDepthRefresh(DepthRefreshSite site, bool depthWrittenSinceLastRefresh, bool copied)
{
    const int index = static_cast<int>(site);
    ++m_counts.depthRefreshes[index];
    if (!depthWrittenSinceLastRefresh)
        ++m_counts.depthRefreshesUnwritten[index];
    if (copied)
        ++m_counts.depthCopies[index];
}

const char* ColourCopySiteName(ColourCopySite site)
{
    const int index = static_cast<int>(site);
    return index >= 0 && index < kColourCopySites ? kColourCopySiteNames[index] : "";
}

const char* DepthRefreshSiteName(DepthRefreshSite site)
{
    const int index = static_cast<int>(site);
    return index >= 0 && index < kDepthRefreshSites ? kDepthRefreshSiteNames[index] : "";
}

void DescribeFrameCopies(const FrameCopyCounts& counts, char* text, size_t size)
{
    if (!text || size == 0)
        return;
    text[0] = '\0';
    size_t used = 0;
    const unsigned frames = counts.frames;
    Append(text, size, used, "frame copies over %u frames, per frame: colour", frames);
    for (int i = 0; i < kColourCopySites; ++i)
        Append(text, size, used, "%s %s %.2f", i ? "," : "", kColourCopySiteNames[i],
               PerFrame(counts.colourCopies[i], frames));
    Append(text, size, used, "; depth refreshes (with no depth write since the last, multisampled copies)");
    for (int i = 0; i < kDepthRefreshSites; ++i)
        Append(text, size, used, "%s %s %.2f (%.2f, %.2f)", i ? "," : "", kDepthRefreshSiteNames[i],
               PerFrame(counts.depthRefreshes[i], frames), PerFrame(counts.depthRefreshesUnwritten[i], frames),
               PerFrame(counts.depthCopies[i], frames));
}

FrameCopyCounters& GlobalFrameCopies()
{
    static FrameCopyCounters counters;
    return counters;
}

void LogFrameCopiesWhenDue()
{
    FrameCopyCounters& counters = GlobalFrameCopies();
    if (!LogEnabled(LogLevel::Info))
    {
        counters.Reset();
        g_summaryStartMs = 0;
        return;
    }
    const ULONGLONG now = GetTickCount64();
    if (!g_summaryStartMs)
        g_summaryStartMs = now;
    if (now - g_summaryStartMs < kSummaryIntervalMs)
        return;
    g_summaryStartMs = now;
    char text[kSummaryTextSize];
    DescribeFrameCopies(counters.Counts(), text, sizeof(text));
    VF_LOG_INFO("%s", text);
    counters.Reset();
}

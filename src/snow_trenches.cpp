/*
 * Copyright (C) 2026 Morfium-G
 * The trenches (units pressing the snow down with a rim beside them that slowly fills back in) derive from
 * seyris-living-azeroth (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU General Public
 * License version 3.
 */
#include "snow_trenches.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kPi = 3.1415927f;
constexpr int kHalfWindow = kTrenchTexels / 2;
constexpr float kByteScale = 255.0f;
constexpr uint32_t kOpaque = 0xFF000000u;
constexpr int kPressShift = 16;
constexpr int kRimShift = 8;
constexpr float kLuminanceRed = 0.2126f;
constexpr float kLuminanceGreen = 0.7152f;
constexpr float kLuminanceBlue = 0.0722f;

int Wrapped(int value)
{
    const int index = value % kTrenchTexels;
    return index < 0 ? index + kTrenchTexels : index;
}

int WorldTexel(float yards)
{
    return static_cast<int>(std::floor(yards / kTrenchTexelYards));
}

float TexelCentre(int texel)
{
    return (texel + 0.5f) * kTrenchTexelYards;
}

float SegmentDistance(const float* from, const float* to, float x, float y)
{
    const float dx = to[0] - from[0];
    const float dy = to[1] - from[1];
    const float length = dx * dx + dy * dy;
    float t = length > 0.0f ? ((x - from[0]) * dx + (y - from[1]) * dy) / length : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    const float ox = x - (from[0] + dx * t);
    const float oy = y - (from[1] + dy * t);
    return std::sqrt(ox * ox + oy * oy);
}

uint32_t Byte(float value)
{
    return static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * kByteScale + 0.5f);
}

constexpr size_t kTexelCount = static_cast<size_t>(kTrenchTexels) * kTrenchTexels;
}

float TrenchPressAt(float distance, float radius)
{
    if (radius <= 0.0f || distance >= radius)
        return 0.0f;
    const float t = std::clamp((1.0f - distance / radius) / (1.0f - kTrenchFloorShare), 0.0f, 1.0f);
    return kTrenchPress * t * t * (3.0f - 2.0f * t);
}

float TrenchRimAt(float distance, float radius)
{
    const float inner = kTrenchRimInner * radius;
    const float outer = kTrenchRimOuter * radius;
    if (radius <= 0.0f || distance <= inner || distance >= outer)
        return 0.0f;
    return std::sin(kPi * (distance - inner) / (outer - inner));
}

TrenchMap::TrenchMap()
    : m_press(kTexelCount, 0.0f), m_rim(kTexelCount, 0.0f), m_fade(kTexelCount, 0), m_texels(kTexelCount, kOpaque),
      m_rowActive(kTrenchTexels, 0), m_changedRows(kTrenchTexels, 0), m_packedRows(kTrenchTexels, 0)
{
}

size_t TrenchMap::IndexOf(int worldI, int worldJ) const
{
    return static_cast<size_t>(Wrapped(worldJ)) * kTrenchTexels + Wrapped(worldI);
}

bool TrenchMap::InWindow(int worldI, int worldJ) const
{
    return m_placed && worldI >= m_originI && worldI < m_originI + kTrenchTexels && worldJ >= m_originJ &&
           worldJ < m_originJ + kTrenchTexels;
}

void TrenchMap::MarkRow(int row)
{
    if (!m_changedRows[row])
        ++m_changedRowCount;
    m_changedRows[row] = 1;
    m_packedRows[row] = 1;
}

void TrenchMap::Reset()
{
    for (int row = 0; row < kTrenchTexels; ++row)
        if (m_rowActive[row])
        {
            const size_t first = static_cast<size_t>(row) * kTrenchTexels;
            std::fill(m_press.begin() + first, m_press.begin() + first + kTrenchTexels, 0.0f);
            std::fill(m_rim.begin() + first, m_rim.begin() + first + kTrenchTexels, 0.0f);
            m_rowActive[row] = 0;
            MarkRow(row);
        }
    m_placed = false;
    m_active = 0;
    m_pendingRelax = 0.0f;
    ++m_generation;
}

void TrenchMap::ClearOutside(int originI, int originJ)
{
    for (int v = 0; v < kTrenchTexels; ++v)
    {
        if (!m_rowActive[v])
            continue;
        const int worldJ = m_originJ + Wrapped(v - m_originJ);
        const bool rowLeaves = worldJ < originJ || worldJ >= originJ + kTrenchTexels;
        for (int u = 0; u < kTrenchTexels; ++u)
        {
            const int worldI = m_originI + Wrapped(u - m_originI);
            if (!rowLeaves && worldI >= originI && worldI < originI + kTrenchTexels)
                continue;
            const size_t index = static_cast<size_t>(v) * kTrenchTexels + u;
            if (m_press[index] <= 0.0f && m_rim[index] <= 0.0f)
                continue;
            m_press[index] = 0.0f;
            m_rim[index] = 0.0f;
            MarkRow(v);
        }
    }
}

void TrenchMap::Place(float x, float y)
{
    const int originI = WorldTexel(x) - kHalfWindow;
    const int originJ = WorldTexel(y) - kHalfWindow;
    if (m_placed)
    {
        const float centreX = (m_originI + kHalfWindow) * kTrenchTexelYards;
        const float centreY = (m_originJ + kHalfWindow) * kTrenchTexelYards;
        if (std::fabs(x - centreX) <= kTrenchRecentreYards && std::fabs(y - centreY) <= kTrenchRecentreYards)
            return;
        ClearOutside(originI, originJ);
    }
    m_originI = originI;
    m_originJ = originJ;
    m_placed = true;
    Count();
    ++m_generation;
}

template <typename Shape>
void TrenchMap::StampArea(float lowX, float lowY, float highX, float highY, float share, uint8_t fade, Shape shape)
{
    if (!m_placed || share <= 0.0f)
        return;
    const int lowI = std::max(WorldTexel(lowX), m_originI);
    const int highI = std::min(WorldTexel(highX), m_originI + kTrenchTexels - 1);
    const int lowJ = std::max(WorldTexel(lowY), m_originJ);
    const int highJ = std::min(WorldTexel(highY), m_originJ + kTrenchTexels - 1);
    bool changed = false;
    for (int j = lowJ; j <= highJ; ++j)
        for (int i = lowI; i <= highI; ++i)
        {
            const float distance = shape(TexelCentre(i), TexelCentre(j));
            const float press = share * TrenchPressAt(distance, 1.0f);
            const float rim = share * TrenchRimAt(distance, 1.0f);
            if (press <= 0.0f && rim <= 0.0f)
                continue;
            const size_t index = IndexOf(i, j);
            const float pressed = std::max(m_press[index], press);
            const float raised = std::min(std::max(m_rim[index], rim), 1.0f - pressed);
            if (pressed == m_press[index] && raised == m_rim[index])
                continue;
            const int row = Wrapped(j);
            if (m_press[index] <= 0.0f && m_rim[index] <= 0.0f)
            {
                ++m_active;
                ++m_rowActive[row];
            }
            m_press[index] = pressed;
            m_rim[index] = raised;
            m_fade[index] = fade;
            MarkRow(row);
            changed = true;
        }
    if (changed)
        ++m_generation;
}

void TrenchMap::StampPrint(const TrenchPrint& print)
{
    if (print.halfLength <= 0.0f || print.halfWidth <= 0.0f)
        return;
    const float reach = kTrenchRimOuter * std::max(print.halfLength, print.halfWidth);
    const float c = std::cos(print.angle);
    const float s = std::sin(print.angle);
    const float x = print.centre[0];
    const float y = print.centre[1];
    StampArea(x - reach, y - reach, x + reach, y + reach, print.share, print.fade, [&](float px, float py) {
        const float along = ((px - x) * c + (py - y) * s) / print.halfLength;
        const float across = ((py - y) * c - (px - x) * s) / print.halfWidth;
        return std::sqrt(along * along + across * across);
    });
}

void TrenchMap::StampScuff(const TrenchScuff& scuff)
{
    if (scuff.radius <= 0.0f)
        return;
    const float reach = kTrenchRimOuter * scuff.radius;
    StampArea(std::min(scuff.from[0], scuff.to[0]) - reach, std::min(scuff.from[1], scuff.to[1]) - reach,
              std::max(scuff.from[0], scuff.to[0]) + reach, std::max(scuff.from[1], scuff.to[1]) + reach, scuff.share,
              scuff.fade,
              [&](float px, float py) { return SegmentDistance(scuff.from, scuff.to, px, py) / scuff.radius; });
}

bool TrenchMap::Relax(float seconds, float relaxSeconds)
{
    if (!m_active)
    {
        m_pendingRelax = 0.0f;
        return false;
    }
    m_pendingRelax += std::max(seconds, 0.0f);
    if (m_pendingRelax < kTrenchRelaxStepSeconds)
        return false;
    const float amount = relaxSeconds > 0.0f ? m_pendingRelax / relaxSeconds : 1.0f;
    m_pendingRelax = 0.0f;
    for (int row = 0; row < kTrenchTexels; ++row)
    {
        if (!m_rowActive[row])
            continue;
        int active = 0;
        for (size_t i = static_cast<size_t>(row) * kTrenchTexels, end = i + kTrenchTexels; i < end; ++i)
        {
            m_press[i] = std::max(m_press[i] - amount, 0.0f);
            m_rim[i] = std::max(m_rim[i] - amount, 0.0f);
            active += m_press[i] > 0.0f || m_rim[i] > 0.0f ? 1 : 0;
        }
        m_active -= m_rowActive[row] - active;
        m_rowActive[row] = active;
        MarkRow(row);
    }
    ++m_generation;
    return true;
}

void TrenchMap::Count()
{
    m_active = 0;
    for (int row = 0; row < kTrenchTexels; ++row)
    {
        int active = 0;
        for (size_t i = static_cast<size_t>(row) * kTrenchTexels, end = i + kTrenchTexels; i < end; ++i)
            active += m_press[i] > 0.0f || m_rim[i] > 0.0f ? 1 : 0;
        m_rowActive[row] = active;
        m_active += active;
    }
}

void TrenchMap::Box(float* box) const
{
    box[0] = m_originI * kTrenchTexelYards;
    box[1] = m_originJ * kTrenchTexelYards;
    box[2] = (m_originI + kTrenchTexels) * kTrenchTexelYards;
    box[3] = (m_originJ + kTrenchTexels) * kTrenchTexelYards;
}

float TrenchMap::PressAt(float x, float y) const
{
    const int i = WorldTexel(x);
    const int j = WorldTexel(y);
    return InWindow(i, j) ? m_press[IndexOf(i, j)] : 0.0f;
}

float TrenchMap::RimAt(float x, float y) const
{
    const int i = WorldTexel(x);
    const int j = WorldTexel(y);
    return InWindow(i, j) ? m_rim[IndexOf(i, j)] : 0.0f;
}

int TrenchMap::FadeAt(float x, float y) const
{
    const int i = WorldTexel(x);
    const int j = WorldTexel(y);
    return InWindow(i, j) ? m_fade[IndexOf(i, j)] : 0;
}

const uint32_t* TrenchMap::Texels()
{
    for (int row = 0; row < kTrenchTexels; ++row)
    {
        if (!m_packedRows[row])
            continue;
        for (size_t i = static_cast<size_t>(row) * kTrenchTexels, end = i + kTrenchTexels; i < end; ++i)
            m_texels[i] = kOpaque | Byte(m_press[i]) << kPressShift | Byte(m_rim[i]) << kRimShift | m_fade[i];
        m_packedRows[row] = 0;
    }
    return m_texels.data();
}

void TrenchMap::MarkUploaded()
{
    std::fill(m_changedRows.begin(), m_changedRows.end(), uint8_t{0});
    m_changedRowCount = 0;
}

TrenchConstants TrenchConstantsFor(const TrenchMap& map, bool active, float rimShare, const TrenchLight& light,
                                   const TrenchReach& reach)
{
    TrenchConstants out = {};
    out.registers[0][0] = 1.0f / kTrenchSpanYards;
    out.registers[0][1] = 1.0f / kTrenchTexels;
    out.registers[0][2] = rimShare;
    out.registers[0][3] = active && map.Placed() ? 1.0f : 0.0f;
    map.Box(out.registers[1]);
    for (int i = 0; i < 3; ++i)
    {
        out.registers[2][i] = light.camera[i];
        out.registers[3][i] = light.toLight[i];
    }
    out.registers[2][3] = kTrenchDepthRangeYards;
    out.registers[3][3] = light.ambientShare;
    out.fade[0] = reach.radius;
    out.fade[1] = reach.fade;
    out.fade[2] = 1.0f / kTrenchFadeSoftYards;
    out.fade[3] = 2.0f * kTrenchTexelYards;
    return out;
}

float AmbientShare(uint32_t ambientColor, uint32_t directColor)
{
    const auto luminance = [](uint32_t colour) {
        return kLuminanceRed * ((colour >> 16) & 0xFFu) + kLuminanceGreen * ((colour >> 8) & 0xFFu) +
               kLuminanceBlue * (colour & 0xFFu);
    };
    const float ambient = luminance(ambientColor);
    const float total = ambient + luminance(directColor);
    return total > 0.0f ? ambient / total : 1.0f;
}

bool TrenchDepthRow(const float* glProjection, float minZ, float maxZ, float* row)
{
    const float extent = maxZ - minZ;
    if (!(extent > 0.0f) || minZ < 0.0f || maxZ > 1.0f || !std::isfinite(glProjection[10]) ||
        !std::isfinite(glProjection[14]) || glProjection[14] == 0.0f)
        return false;
    row[0] = minZ + extent * (1.0f + glProjection[10]) * 0.5f;
    row[1] = extent * glProjection[14] * 0.5f;
    row[2] = kTrenchDepthPushBack;
    row[3] = 0.0f;
    return true;
}

TrenchMap& GlobalTrenchMap()
{
    static TrenchMap map;
    return map;
}

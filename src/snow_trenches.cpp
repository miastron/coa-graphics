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
}

float TrenchRadius(float collisionRadius)
{
    return std::clamp(collisionRadius * kTrenchRadiusScale, kTrenchMinRadius, kTrenchMaxRadius);
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
    : m_press(static_cast<size_t>(kTrenchTexels) * kTrenchTexels, 0.0f),
      m_rim(static_cast<size_t>(kTrenchTexels) * kTrenchTexels, 0.0f),
      m_texels(static_cast<size_t>(kTrenchTexels) * kTrenchTexels, kOpaque)
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

void TrenchMap::Reset()
{
    std::fill(m_press.begin(), m_press.end(), 0.0f);
    std::fill(m_rim.begin(), m_rim.end(), 0.0f);
    m_placed = false;
    m_active = 0;
    m_pendingRelax = 0.0f;
    ++m_generation;
}

void TrenchMap::ClearOutside(int originI, int originJ)
{
    for (int u = 0; u < kTrenchTexels; ++u)
    {
        const int worldI = m_originI + Wrapped(u - m_originI);
        if (worldI >= originI && worldI < originI + kTrenchTexels)
            continue;
        for (int v = 0; v < kTrenchTexels; ++v)
        {
            const size_t index = static_cast<size_t>(v) * kTrenchTexels + u;
            m_press[index] = 0.0f;
            m_rim[index] = 0.0f;
        }
    }
    for (int v = 0; v < kTrenchTexels; ++v)
    {
        const int worldJ = m_originJ + Wrapped(v - m_originJ);
        if (worldJ >= originJ && worldJ < originJ + kTrenchTexels)
            continue;
        std::fill(m_press.begin() + static_cast<size_t>(v) * kTrenchTexels,
                  m_press.begin() + static_cast<size_t>(v + 1) * kTrenchTexels, 0.0f);
        std::fill(m_rim.begin() + static_cast<size_t>(v) * kTrenchTexels,
                  m_rim.begin() + static_cast<size_t>(v + 1) * kTrenchTexels, 0.0f);
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

void TrenchMap::Stamp(const float* from, const float* to, float radius)
{
    if (!m_placed || radius <= 0.0f)
        return;
    const float reach = kTrenchRimOuter * radius;
    const int lowI = std::max(WorldTexel(std::min(from[0], to[0]) - reach), m_originI);
    const int highI = std::min(WorldTexel(std::max(from[0], to[0]) + reach), m_originI + kTrenchTexels - 1);
    const int lowJ = std::max(WorldTexel(std::min(from[1], to[1]) - reach), m_originJ);
    const int highJ = std::min(WorldTexel(std::max(from[1], to[1]) + reach), m_originJ + kTrenchTexels - 1);
    bool changed = false;
    for (int j = lowJ; j <= highJ; ++j)
        for (int i = lowI; i <= highI; ++i)
        {
            const float distance = SegmentDistance(from, to, TexelCentre(i), TexelCentre(j));
            const float press = TrenchPressAt(distance, radius);
            const float rim = TrenchRimAt(distance, radius);
            if (press <= 0.0f && rim <= 0.0f)
                continue;
            const size_t index = IndexOf(i, j);
            const float pressed = std::max(m_press[index], press);
            const float raised = std::min(std::max(m_rim[index], rim), 1.0f - pressed);
            if (pressed == m_press[index] && raised == m_rim[index])
                continue;
            if (m_press[index] <= 0.0f && m_rim[index] <= 0.0f)
                ++m_active;
            m_press[index] = pressed;
            m_rim[index] = raised;
            changed = true;
        }
    if (changed)
        ++m_generation;
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
    for (size_t i = 0; i < m_press.size(); ++i)
    {
        m_press[i] = std::max(m_press[i] - amount, 0.0f);
        m_rim[i] = std::max(m_rim[i] - amount, 0.0f);
    }
    Count();
    ++m_generation;
    return true;
}

void TrenchMap::Count()
{
    m_active = 0;
    for (size_t i = 0; i < m_press.size(); ++i)
        m_active += m_press[i] > 0.0f || m_rim[i] > 0.0f ? 1 : 0;
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

const uint32_t* TrenchMap::Texels()
{
    if (m_packed != m_generation)
    {
        for (size_t i = 0; i < m_texels.size(); ++i)
            m_texels[i] = kOpaque | Byte(m_press[i]) << kPressShift | Byte(m_rim[i]) << kRimShift;
        m_packed = m_generation;
    }
    return m_texels.data();
}

unsigned TrenchStamper::Step(TrenchMap& map, const NearbyUnitFrame& units, const bool* onGround)
{
    for (auto& walker : m_walkers)
        walker.second.seen = false;
    unsigned stamps = 0;
    for (uint32_t i = 0; i < units.count; ++i)
    {
        const NearbyUnit& unit = units.units[i];
        if (!onGround[i])
        {
            m_walkers.erase(unit.guid);
            continue;
        }
        const float here[2] = {unit.position[0], unit.position[1]};
        const float radius = TrenchRadius(unit.radius);
        auto found = m_walkers.find(unit.guid);
        if (found != m_walkers.end())
        {
            const float dx = here[0] - found->second.position[0];
            const float dy = here[1] - found->second.position[1];
            const bool jumped = dx * dx + dy * dy > kTrenchJumpYards * kTrenchJumpYards;
            map.Stamp(jumped ? here : found->second.position, here, radius);
        }
        else
        {
            map.Stamp(here, here, radius);
        }
        m_walkers[unit.guid] = {{here[0], here[1]}, true};
        ++stamps;
    }
    for (auto it = m_walkers.begin(); it != m_walkers.end();)
        it = it->second.seen ? std::next(it) : m_walkers.erase(it);
    return stamps;
}

TrenchConstants TrenchConstantsFor(const TrenchMap& map, bool active, float rimShare, const TrenchLight& light)
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

TrenchStamper& GlobalTrenchStamper()
{
    static TrenchStamper stamper;
    return stamper;
}

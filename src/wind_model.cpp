/*
 * Copyright (C) 2026 Morfium-G
 * Derived from seyris-living-azeroth (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU
 * General Public License version 3; see LICENSE.
 */
#include "wind_model.h"

#include "wdbc.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace wind
{
namespace
{
constexpr float kPi = 3.14159265358979f;
constexpr float kDegreesToRadians = kPi / 180.0f;
constexpr float kMaxFrameSeconds = 1.0f;
constexpr float kFallbackFrameSeconds = 1.0f / 60.0f;
constexpr float kBlendSeconds = 1.5f;
constexpr float kRangeWanderSeconds = 40.0f;
constexpr float kRangeWanderOffset = 17.3f;
constexpr float kRangeFloor = 0.2f;
constexpr float kRangeWander = 0.5f;
constexpr float kBearingWanderSeconds = 90.0f;
constexpr float kBearingWanderOffset = 3.1f;
constexpr float kBearingWanderDegrees = 60.0f;
constexpr float kMapBearingSalt = 91.0f;
constexpr float kFullTurnDegrees = 360.0f;
constexpr float kHalfTurnDegrees = 180.0f;
constexpr float kMinGustFrequency = 0.05f;
constexpr float kGustAlongYards = 50.0f;
constexpr float kGustAcrossYards = 35.0f;
constexpr float kGustBaseSpeed = 4.0f;
constexpr float kGustSpeedPerStrength = 16.0f;
constexpr float kGustContrast = 3.0f;
constexpr float kVeerYards = 80.0f;
constexpr float kVeerOffset = 31.0f;
constexpr float kVeerDegrees = 20.0f;
constexpr uint32_t kHashMask = (1u << 24) - 1u;
constexpr uint32_t kProfileFields = 3 + FieldCount;
constexpr uint32_t kProfileScopeTypeColumn = 1;
constexpr uint32_t kProfileScopeIdColumn = 2;
constexpr uint32_t kProfileFirstValueColumn = 3;

float Clamp01(float x)
{
    return std::min(std::max(x, 0.0f), 1.0f);
}

float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float AngleDelta(float from, float to)
{
    float d = std::fmod(to - from, kFullTurnDegrees);
    if (d > kHalfTurnDegrees)
        d -= kFullTurnDegrees;
    if (d < -kHalfTurnDegrees)
        d += kFullTurnDegrees;
    return d;
}

float Hash(int32_t x, int32_t y)
{
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & kHashMask) / static_cast<float>(kHashMask);
}

float Smooth(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

float Noise1(float x)
{
    const float cell = std::floor(x);
    const int32_t i = static_cast<int32_t>(cell);
    return Lerp(Hash(i, 0), Hash(i + 1, 0), Smooth(x - cell));
}

float Noise2(float x, float y)
{
    const float fx = std::floor(x);
    const float fy = std::floor(y);
    const int32_t ix = static_cast<int32_t>(fx);
    const int32_t iy = static_cast<int32_t>(fy);
    const float tx = Smooth(x - fx);
    const float ty = Smooth(y - fy);
    const float a = Lerp(Hash(ix, iy), Hash(ix + 1, iy), tx);
    const float b = Lerp(Hash(ix, iy + 1), Hash(ix + 1, iy + 1), tx);
    return Lerp(a, b, ty);
}

float Fbm2(float x, float y)
{
    return 0.55f * Noise2(x, y) + 0.3f * Noise2(x * 2.1f + 7.3f, y * 2.1f - 3.1f) +
           0.15f * Noise2(x * 4.3f - 1.7f, y * 4.3f + 5.9f);
}

float MapBaseBearing(int mapId)
{
    return Hash(mapId, static_cast<int32_t>(kMapBearingSalt)) * kFullTurnDegrees;
}

Row BuiltInGlobalRow()
{
    Row row;
    const float values[FieldCount] = {0.3f, 0.65f, 0.4f, 0.8f, 40.0f, 0.8f, 0.6f, 1.0f, 1.0f, 0.0f, 0.0f};
    std::memcpy(row.value, values, sizeof(values));
    return row;
}
}

float WeatherIntensity(float rawIntensity)
{
    return Clamp01((rawIntensity - kWeatherKnee) / (1.0f - kWeatherKnee));
}

int AreaChain(uint32_t area, AreaParentFn parentOf, uint32_t* out, int capacity)
{
    int count = 0;
    while (area && count < capacity)
    {
        out[count++] = area;
        const uint32_t parent = parentOf(area);
        if (parent == area)
            break;
        area = parent;
    }
    return count;
}

WindModel::WindModel()
{
    UseBuiltInProfiles();
}

bool WindModel::LoadProfiles(const std::string& path, std::string& error)
{
    WdbcTable table;
    if (!table.Load(path, error))
        return false;
    if (table.Fields() < kProfileFields)
    {
        error = "WindProfile has too few columns";
        return false;
    }
    std::vector<Row> rows;
    for (uint32_t r = 0; r < table.Records(); ++r)
    {
        Row row;
        row.scopeType = table.Int(r, kProfileScopeTypeColumn);
        row.scopeId = table.Int(r, kProfileScopeIdColumn);
        for (int f = 0; f < FieldCount; ++f)
            row.value[f] = table.Float(r, kProfileFirstValueColumn + f);
        rows.push_back(row);
    }
    SetProfiles(std::move(rows));
    return true;
}

void WindModel::SetProfiles(std::vector<Row> rows)
{
    m_rows = std::move(rows);
    m_builtIn = false;
    m_haveCurrent = false;
}

void WindModel::UseBuiltInProfiles()
{
    m_rows.assign(1, BuiltInGlobalRow());
    m_builtIn = true;
    m_haveCurrent = false;
}

void WindModel::SetStrength(float multiplier)
{
    m_strength = std::max(multiplier, 0.0f);
}

WindModel::Resolved WindModel::Resolve(const Place& place) const
{
    Resolved resolved;
    auto apply = [&](int scopeType, int32_t scopeId) {
        for (const Row& row : m_rows)
        {
            if (row.scopeType != scopeType || (scopeType != ScopeGlobal && row.scopeId != scopeId))
                continue;
            for (int f = 0; f < FieldCount; ++f)
                if (row.value[f] >= 0.0f)
                    resolved.value[f] = row.value[f];
            return;
        }
    };
    apply(ScopeGlobal, 0);
    apply(ScopeMap, place.mapId);
    for (int i = std::min(place.areaCount, kMaxAreaChain) - 1; i >= 0; --i)
        apply(ScopeArea, static_cast<int32_t>(place.areaChain[i]));
    return resolved;
}

void WindModel::BlendToward(float dt)
{
    if (!m_haveCurrent)
    {
        m_current = m_target;
        m_haveCurrent = true;
        return;
    }
    const float k = 1.0f - std::exp(-dt / kBlendSeconds);
    for (int f = 0; f < FieldCount; ++f)
        if (f == LockedDirection)
            m_current.value[f] += AngleDelta(m_current.value[f], m_target.value[f]) * k;
        else
            m_current.value[f] = Lerp(m_current.value[f], m_target.value[f], k);
}

void WindModel::Update(float dt, const Place& place)
{
    if (!(dt >= 0.0f && dt <= kMaxFrameSeconds))
        dt = kFallbackFrameSeconds;
    m_time += dt;
    m_target = Resolve(place);
    BlendToward(dt);
    const float* p = m_current.value;

    float rangePosition = kRangeFloor + kRangeWander * Noise1(m_time / kRangeWanderSeconds + kRangeWanderOffset);
    rangePosition += (1.0f - rangePosition) * Clamp01(place.weather * p[WeatherInfluence]);
    m_steady = Lerp(p[GroundMin], p[GroundMax], rangePosition);

    const float wander = Noise1(m_time / kBearingWanderSeconds + kBearingWanderOffset) * 2.0f - 1.0f;
    float bearing = MapBaseBearing(place.mapId) + kBearingWanderDegrees * wander;
    bearing += AngleDelta(bearing, p[LockedDirection]) * Clamp01(p[LockStrength]);
    m_bearing = std::fmod(bearing + kFullTurnDegrees, kFullTurnDegrees);
}

Sample WindModel::At(const float position[3]) const
{
    const float* p = m_current.value;
    const float radians = m_bearing * kDegreesToRadians;
    const float dx = std::cos(radians);
    const float dy = -std::sin(radians);
    const float px = -dy;
    const float py = dx;

    const float frequency = std::max(p[GustFrequency], kMinGustFrequency);
    const float speed = kGustBaseSpeed + kGustSpeedPerStrength * m_steady;
    const float u = position[0] * dx + position[1] * dy - m_time * speed;
    const float v = position[0] * px + position[1] * py;
    const float along = kGustAlongYards / frequency;
    const float across = kGustAcrossYards / frequency;

    Sample sample;
    sample.gust = Clamp01((Fbm2(u / along, v / across) - 0.5f) * kGustContrast);
    const float veer = (Noise2(u / kVeerYards + kVeerOffset, v / kVeerYards) - 0.5f) * kVeerDegrees *
                       (0.5f + sample.gust) * kDegreesToRadians;
    sample.dirX = dx * std::cos(veer) - dy * std::sin(veer);
    sample.dirY = dx * std::sin(veer) + dy * std::cos(veer);
    sample.strength = m_steady * (1.0f + p[GustStrength] * sample.gust) * m_strength;
    return sample;
}

WindModel& GlobalWindModel()
{
    static WindModel model;
    return model;
}
}

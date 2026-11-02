/*
 * Copyright (C) 2026 Morfium-G
 * Derived from seyris-living-azeroth (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU
 * General Public License version 3; see LICENSE.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace wind
{
constexpr int kMaxAreaChain = 8;
constexpr float kWeatherKnee = 0.25f;

enum Field
{
    GroundMin,
    GroundMax,
    AloftMin,
    AloftMax,
    AloftHeight,
    WeatherInfluence,
    GustStrength,
    GustFrequency,
    TerrainWeight,
    LockedDirection,
    LockStrength,
    FieldCount
};

enum Scope
{
    ScopeGlobal = 0,
    ScopeMap = 1,
    ScopeArea = 2
};

struct Row
{
    int32_t scopeType = ScopeGlobal;
    int32_t scopeId = 0;
    float value[FieldCount] = {};
};

struct Place
{
    int mapId = -1;
    uint32_t areaChain[kMaxAreaChain] = {};
    int areaCount = 0;
    float weather = 0.0f;
};

struct Sample
{
    float dirX = 1.0f;
    float dirY = 0.0f;
    float strength = 0.0f;
    float gust = 0.0f;
};

using AreaParentFn = uint32_t (*)(uint32_t area);

float WeatherIntensity(float rawIntensity);
int AreaChain(uint32_t area, AreaParentFn parentOf, uint32_t* out, int capacity);

class WindModel
{
public:
    WindModel();

    bool LoadProfiles(const std::string& path, std::string& error);
    void SetProfiles(std::vector<Row> rows);
    void UseBuiltInProfiles();
    void SetStrength(float multiplier);
    void Update(float dt, const Place& place);
    Sample At(const float position[3]) const;

    const float* Profile() const { return m_current.value; }
    float Steady() const { return m_steady; }
    float Bearing() const { return m_bearing; }
    size_t RowCount() const { return m_rows.size(); }
    bool BuiltIn() const { return m_builtIn; }

private:
    struct Resolved
    {
        float value[FieldCount] = {};
    };

    Resolved Resolve(const Place& place) const;
    void BlendToward(float dt);

    std::vector<Row> m_rows;
    bool m_builtIn = true;
    Resolved m_target;
    Resolved m_current;
    bool m_haveCurrent = false;
    float m_time = 0.0f;
    float m_strength = 1.0f;
    float m_steady = 0.0f;
    float m_bearing = 0.0f;
};

WindModel& GlobalWindModel();
}

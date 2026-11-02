/*
 * Copyright (C) 2026 Morfium-G
 * The trenches (units pressing the snow down with a rim beside them that slowly fills back in) derive from
 * seyris-living-azeroth (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU General Public
 * License version 3.
 */
#pragma once

#include "engine_actors.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

constexpr int kTrenchTexels = 256;
constexpr float kTrenchTexelYards = 0.25f;
constexpr float kTrenchSpanYards = kTrenchTexels * kTrenchTexelYards;
constexpr float kTrenchRecentreYards = 8.0f;
constexpr float kTrenchPress = 0.9f;
constexpr float kTrenchFloorShare = 0.35f;
constexpr float kTrenchRimInner = 0.8f;
constexpr float kTrenchRimOuter = 1.7f;
constexpr float kTrenchRadiusScale = 1.3f;
constexpr float kTrenchMinRadius = 0.3f;
constexpr float kTrenchMaxRadius = 1.5f;
constexpr float kTrenchGroundTolerance = 0.6f;
constexpr float kTrenchJumpYards = 3.0f;
constexpr float kTrenchUnitRange = 30.0f;
constexpr float kTrenchRelaxStepSeconds = 0.1f;
constexpr float kTrenchDepthRangeYards = 2.0f;

float TrenchRadius(float collisionRadius);
float TrenchPressAt(float distance, float radius);
float TrenchRimAt(float distance, float radius);

class TrenchMap
{
public:
    TrenchMap();

    void Place(float x, float y);
    void Stamp(const float* from, const float* to, float radius);
    bool Relax(float seconds, float relaxSeconds);
    void Reset();
    bool Placed() const { return m_placed; }
    void Box(float* box) const;
    float PressAt(float x, float y) const;
    float RimAt(float x, float y) const;
    int ActiveTexels() const { return m_active; }
    unsigned Generation() const { return m_generation; }
    const uint32_t* Texels();

private:
    size_t IndexOf(int worldI, int worldJ) const;
    bool InWindow(int worldI, int worldJ) const;
    void ClearOutside(int originI, int originJ);
    void Count();

    std::vector<float> m_press;
    std::vector<float> m_rim;
    std::vector<uint32_t> m_texels;
    int m_originI = 0;
    int m_originJ = 0;
    int m_active = 0;
    float m_pendingRelax = 0.0f;
    unsigned m_generation = 0;
    unsigned m_packed = ~0u;
    bool m_placed = false;
};

struct TrenchWalker
{
    float position[2];
    bool seen;
};

class TrenchStamper
{
public:
    unsigned Step(TrenchMap& map, const NearbyUnitFrame& units, const bool* onGround);
    void Reset() { m_walkers.clear(); }

private:
    std::unordered_map<uint64_t, TrenchWalker> m_walkers;
};

constexpr unsigned kTrenchPixelRegister = 204;
constexpr unsigned kTrenchPixelRegisterCount = 4;
constexpr unsigned kTrenchPixelSampler = 10;
constexpr unsigned kTrenchDepthRegister = 210;
constexpr float kTrenchDepthPushBack = 1.00001f;

struct TrenchConstants
{
    float registers[4][4];
};

struct TrenchLight
{
    float camera[3];
    float toLight[3];
    float ambientShare;
};

TrenchConstants TrenchConstantsFor(const TrenchMap& map, bool active, float rimShare, const TrenchLight& light);
float AmbientShare(uint32_t ambientColor, uint32_t directColor);
bool TrenchDepthRow(const float* glProjection, float minZ, float maxZ, float* row);

TrenchMap& GlobalTrenchMap();
TrenchStamper& GlobalTrenchStamper();

/*
 * Copyright (C) 2026 Morfium-G
 * The trenches (units pressing the snow down with a rim beside them that slowly fills back in) derive from
 * seyris-living-azeroth (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU General Public
 * License version 3.
 */
#pragma once

#include <cstdint>
#include <vector>

constexpr int kTrenchTexels = 1024;
constexpr float kTrenchTexelYards = 0.0625f;
constexpr float kTrenchSpanYards = kTrenchTexels * kTrenchTexelYards;
constexpr float kTrenchRecentreYards = 8.0f;
constexpr float kTrenchPress = 0.9f;
constexpr float kTrenchFloorShare = 0.35f;
constexpr float kTrenchRimInner = 0.8f;
constexpr float kTrenchRimOuter = 1.7f;
constexpr float kTrenchGroundTolerance = 0.6f;
constexpr float kTrenchUnitRange = 30.0f;
constexpr float kTrenchRelaxStepSeconds = 0.25f;
constexpr float kTrenchDepthRangeYards = 1.2f;
constexpr float kTrenchFadeSoftYards = 1.5f;

float TrenchPressAt(float distance, float radius);
float TrenchRimAt(float distance, float radius);

struct TrenchPrint
{
    float centre[2];
    float angle;
    float halfLength;
    float halfWidth;
    float share;
    uint8_t fade;
};

struct TrenchScuff
{
    float from[2];
    float to[2];
    float radius;
    float share;
    uint8_t fade;
};

class TrenchMap
{
public:
    TrenchMap();

    void Place(float x, float y);
    void StampPrint(const TrenchPrint& print);
    void StampScuff(const TrenchScuff& scuff);
    bool Relax(float seconds, float relaxSeconds);
    void Reset();
    bool Placed() const { return m_placed; }
    void Box(float* box) const;
    float PressAt(float x, float y) const;
    float RimAt(float x, float y) const;
    int FadeAt(float x, float y) const;
    int ActiveTexels() const { return m_active; }
    unsigned Generation() const { return m_generation; }
    const uint32_t* Texels();
    bool RowChanged(int row) const { return m_changedRows[row] != 0; }
    int ChangedRows() const { return m_changedRowCount; }
    void MarkUploaded();

private:
    template <typename Shape>
    void StampArea(float lowX, float lowY, float highX, float highY, float share, uint8_t fade, Shape shape);
    size_t IndexOf(int worldI, int worldJ) const;
    bool InWindow(int worldI, int worldJ) const;
    void ClearOutside(int originI, int originJ);
    void MarkRow(int row);
    void Count();

    std::vector<float> m_press;
    std::vector<float> m_rim;
    std::vector<uint8_t> m_fade;
    std::vector<uint32_t> m_texels;
    std::vector<int> m_rowActive;
    std::vector<uint8_t> m_changedRows;
    std::vector<uint8_t> m_packedRows;
    int m_changedRowCount = 0;
    int m_originI = 0;
    int m_originJ = 0;
    int m_active = 0;
    float m_pendingRelax = 0.0f;
    unsigned m_generation = 0;
    bool m_placed = false;
};

constexpr unsigned kTrenchPixelRegister = 204;
constexpr unsigned kTrenchPixelRegisterCount = 4;
constexpr unsigned kTrenchPixelSampler = 10;
constexpr unsigned kTrenchDepthRegister = 210;
constexpr unsigned kTrenchFadeRegister = 211;
constexpr float kTrenchDepthPushBack = 1.00001f;

struct TrenchConstants
{
    float registers[4][4];
    float fade[4];
};

struct TrenchLight
{
    float camera[3];
    float toLight[3];
    float ambientShare;
};

struct TrenchReach
{
    float radius;
    float fade;
};

TrenchConstants TrenchConstantsFor(const TrenchMap& map, bool active, float rimShare, const TrenchLight& light,
                                   const TrenchReach& reach);
float AmbientShare(uint32_t ambientColor, uint32_t directColor);
bool TrenchDepthRow(const float* glProjection, float minZ, float maxZ, float* row);

TrenchMap& GlobalTrenchMap();

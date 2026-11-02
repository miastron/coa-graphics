/*
 * Copyright (C) 2026 Morfium-G
 * The snow rules (painted snow ground, slope limit, drifts, ragged edges) derive from seyris-living-azeroth
 * (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU General Public License version 3.
 */
#pragma once

#include "config.h"
#include "snow_trenches.h"
#include "terrain_shadows.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct SnowSettings
{
    bool enabled;
    float depth;
    float maxSlope;
    float slopeFade;
    float drift;
    float edges;
    float breakup;
};

SnowSettings SnowSettingsFrom(const Config& config);
bool SameSnowShape(const SnowSettings& a, const SnowSettings& b);

constexpr int kSnowTerrainTypeSnow = 3;
constexpr int kSnowAlphaSide = 64;
constexpr int kSnowAlphaTexels = kSnowAlphaSide * kSnowAlphaSide;
constexpr int kSnowMaxLayers = 4;
constexpr uint32_t kSnowLayerHasAlpha = 0x100;
constexpr uint32_t kSnowLayerCompressed = 0x200;
constexpr int kSnowWindowChunks = 16;
constexpr int kSnowTexelsPerChunk = 32;
constexpr int kSnowAlphaPerTexel = kSnowAlphaSide / kSnowTexelsPerChunk;
constexpr int kSnowMapTexels = kSnowWindowChunks * kSnowTexelsPerChunk;
constexpr float kSnowTexelYards = kTerrainChunkYards / kSnowTexelsPerChunk;
constexpr float kSnowMapYards = kSnowTexelYards * kSnowMapTexels;
constexpr float kSnowMaxDepthYards = 2.0f;
constexpr float kSnowEdgeFadeYards = 16.0f;
constexpr int kSnowChunksPerFrame = 8;
constexpr int kSnowErosionTexels = 2;

bool SnowLayer(int terrainType, const char* texturePath);
size_t DecodeTerrainAlpha(const unsigned char* source, size_t available, uint32_t layerFlags, bool eightBit,
                          bool repeatLastRowAndColumn, unsigned char* out);

struct SnowChunk
{
    unsigned char snow[kSnowAlphaTexels];
    float heights[kTerrainChunkVertexSide][kTerrainChunkVertexSide];
};

class SnowSource
{
public:
    virtual uintptr_t ChunkIdentity(int chunkRow, int chunkCol) const = 0;
    virtual bool ReadChunk(int chunkRow, int chunkCol, SnowChunk& out) const = 0;

protected:
    ~SnowSource() = default;
};

uint32_t SnowTexel(float coverage, float slopeFactor, float depthYards, unsigned noise);
float SnowSlopeFactor(float slopeDegrees, const SnowSettings& settings);
float SnowBase(float coverage, float slopeFactor);
float SnowDepthAt(float base, float drift, const SnowSettings& settings);
int SnowTexelIndex(int worldTexel);
int SnowBlockStart(int chunk);

class SnowMap
{
public:
    SnowMap();

    void Configure(const SnowSettings& settings);
    const SnowSettings& Settings() const { return m_settings; }
    bool Track(const float* cameraWorld, int mapId, const SnowSource& source, int budget);
    void Reset();
    const uint32_t* Texels() const { return m_texels.data(); }
    bool Placed() const { return m_placed; }
    void Box(float* box) const;
    int FilledChunks() const { return m_filled; }
    int SnowChunks() const { return m_snowChunks; }
    int PendingChunks() const { return m_pending; }
    unsigned Generation() const { return m_generation; }

private:
    struct Slot
    {
        int row = 0;
        int col = 0;
        uintptr_t identity = 0;
        bool occupied = false;
        bool scanned = false;
        bool filled = false;
        bool snow = false;
    };

    Slot& SlotOf(int row, int col);
    void Clear(int row, int col);
    void Fill(int row, int col, const SnowChunk* chunk);
    void Shape(int row, int col);
    bool InWindow(int worldRow, int worldCol) const;
    uint32_t& TexelAt(int worldRow, int worldCol);
    float BaseAt(int worldRow, int worldCol);
    void Place(int cameraRow, int cameraCol, int mapId);
    void Count();

    SnowSettings m_settings;
    std::vector<uint32_t> m_texels;
    std::vector<Slot> m_slots;
    int m_firstRow = 0;
    int m_firstCol = 0;
    int m_mapId = -1;
    int m_filled = 0;
    int m_snowChunks = 0;
    int m_pending = 0;
    unsigned m_generation = 0;
    bool m_placed = false;
};

struct SnowVertexConstants
{
    float registers[3][4];
};

struct SnowPixelConstants
{
    float registers[4][4];
};

constexpr unsigned kSnowVertexRegister = 200;
constexpr unsigned kSnowPixelRegister = 220;
constexpr unsigned kSnowVertexRegisterCount = 3;
constexpr unsigned kSnowPixelRegisterCount = 4;
constexpr unsigned kSnowPixelSampler = 9;
constexpr float kSnowDetailScale = 4.7f;
constexpr float kSnowEdgeSharpness = 4.0f;
constexpr float kSnowAlbedo = 0.9f;
constexpr float kSnowShadowGain = 3.0f;

SnowVertexConstants SnowVertexConstantsFor(const SnowMap& map, bool active);
SnowPixelConstants SnowPixelConstantsFor(const SnowMap& map, bool active);

struct SnowFrame
{
    SnowVertexConstants vertex;
    SnowPixelConstants pixel;
    TrenchConstants trench;
    bool trenchDepth = false;
};

SnowMap& GlobalSnowMap();
SnowFrame& GlobalSnowFrame();

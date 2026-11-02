/*
 * Copyright (C) 2026 Morfium-G
 * The snow rules (painted snow ground, slope limit, drifts, ragged edges) derive from seyris-living-azeroth
 * (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU General Public License version 3.
 */
#include "snow_cover.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace
{
constexpr float kRadiansToDegrees = 57.29578f;
constexpr float kDepthCoverageLow = 0.5f;
constexpr float kDepthCoverageHigh = 0.9f;
constexpr int kByteShiftDepth = 16;
constexpr int kByteShiftCoverage = 8;
constexpr int kByteShiftSlope = 24;
constexpr int kDriftLattice = 8;
constexpr int kTexelsPerCell = kSnowTexelsPerChunk / kTerrainChunkCells;
constexpr int kWindowHalf = kSnowWindowChunks / 2;
constexpr int kWorldTexelsFromOrigin = kTerrainMapChunks / 2 * kSnowTexelsPerChunk;
constexpr int kRunLengthFill = 0x80;
constexpr int kRunLengthCount = 0x7F;
constexpr unsigned kNibbleScale = 17;
constexpr float kByteScale = 255.0f;
constexpr uint32_t kHashRowMultiplier = 0x9E3779B1u;
constexpr uint32_t kHashColMultiplier = 0x85EBCA77u;
constexpr uint32_t kHashFirstFinal = 0x85EBCA6Bu;
constexpr uint32_t kHashSecondFinal = 0xC2B2AE35u;
constexpr unsigned kHashShortShift = 13;
constexpr unsigned kHashLongShift = 16;
constexpr const char* kSnowName = "snow";

uint32_t Hash(int i, int j)
{
    uint32_t h = static_cast<uint32_t>(i) * kHashRowMultiplier ^ static_cast<uint32_t>(j) * kHashColMultiplier;
    h ^= h >> kHashLongShift;
    h *= kHashFirstFinal;
    h ^= h >> kHashShortShift;
    h *= kHashSecondFinal;
    h ^= h >> kHashLongShift;
    return h;
}

float HashUnit(int i, int j)
{
    return static_cast<float>(Hash(i, j) & 0xFFFFu) / 65535.0f;
}

int FloorDiv(int value, int divisor)
{
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

float Smooth(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

float DriftNoise(int i, int j)
{
    const int ci = FloorDiv(i, kDriftLattice);
    const int cj = FloorDiv(j, kDriftLattice);
    const float fi = Smooth(static_cast<float>(i - ci * kDriftLattice) / kDriftLattice);
    const float fj = Smooth(static_cast<float>(j - cj * kDriftLattice) / kDriftLattice);
    const float top = HashUnit(ci, cj) + (HashUnit(ci + 1, cj) - HashUnit(ci, cj)) * fi;
    const float bottom = HashUnit(ci, cj + 1) + (HashUnit(ci + 1, cj + 1) - HashUnit(ci, cj + 1)) * fi;
    return top + (bottom - top) * fj;
}

unsigned char UnitByte(float value)
{
    return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * kByteScale + 0.5f);
}

bool InMap(int chunk)
{
    return chunk >= 0 && chunk < kTerrainMapChunks;
}

float CellSlopeDegrees(const SnowChunk& chunk, int a, int b)
{
    const float(&h)[kTerrainChunkVertexSide][kTerrainChunkVertexSide] = chunk.heights;
    const float across = 2.0f * kTerrainVertexYards;
    const float dx = (h[a + 1][b] + h[a + 1][b + 1] - h[a][b] - h[a][b + 1]) / across;
    const float dy = (h[a][b + 1] + h[a + 1][b + 1] - h[a][b] - h[a + 1][b]) / across;
    return std::atan(std::sqrt(dx * dx + dy * dy)) * kRadiansToDegrees;
}

float CellCoordinate(int texel)
{
    return std::clamp((texel + 0.5f) / kTexelsPerCell - 0.5f, 0.0f, static_cast<float>(kTerrainChunkCells - 1));
}
}

SnowSettings SnowSettingsFrom(const Config& c)
{
    return {c.snow, c.snowDepth, c.snowMaxSlope, c.snowSlopeFade, c.snowDrift, c.snowEdges, c.snowBreakup};
}

bool SameSnowShape(const SnowSettings& a, const SnowSettings& b)
{
    return a.depth == b.depth && a.maxSlope == b.maxSlope && a.slopeFade == b.slopeFade && a.drift == b.drift;
}

bool SnowLayer(int terrainType, const char* texturePath)
{
    if (terrainType == kSnowTerrainTypeSnow)
        return true;
    if (!texturePath)
        return false;
    const size_t nameLength = std::strlen(kSnowName);
    for (const char* at = texturePath; *at; ++at)
    {
        size_t matched = 0;
        while (matched < nameLength && at[matched] &&
               std::tolower(static_cast<unsigned char>(at[matched])) == kSnowName[matched])
            ++matched;
        if (matched == nameLength)
            return true;
    }
    return false;
}

size_t DecodeTerrainAlpha(const unsigned char* source, size_t available, uint32_t layerFlags, bool eightBit,
                          bool repeatLastRowAndColumn, unsigned char* out)
{
    if (layerFlags & kSnowLayerCompressed)
    {
        size_t read = 0;
        int written = 0;
        while (written < kSnowAlphaTexels)
        {
            if (read >= available)
                return 0;
            const unsigned char head = source[read++];
            int count = head & kRunLengthCount;
            if (!count)
                return 0;
            if (head & kRunLengthFill)
            {
                if (read >= available)
                    return 0;
                const unsigned char value = source[read++];
                for (; count && written < kSnowAlphaTexels; --count)
                    out[written++] = value;
            }
            else
            {
                for (; count && written < kSnowAlphaTexels; --count)
                {
                    if (read >= available)
                        return 0;
                    out[written++] = source[read++];
                }
            }
        }
        return read;
    }
    if (eightBit)
    {
        if (available < static_cast<size_t>(kSnowAlphaTexels))
            return 0;
        std::memcpy(out, source, kSnowAlphaTexels);
        return kSnowAlphaTexels;
    }
    const size_t packed = kSnowAlphaTexels / 2;
    if (available < packed)
        return 0;
    for (size_t i = 0; i < packed; ++i)
    {
        out[i * 2] = static_cast<unsigned char>((source[i] & 0x0F) * kNibbleScale);
        out[i * 2 + 1] = static_cast<unsigned char>((source[i] >> 4) * kNibbleScale);
    }
    if (repeatLastRowAndColumn)
    {
        for (int row = 0; row < kSnowAlphaSide; ++row)
            out[row * kSnowAlphaSide + kSnowAlphaSide - 1] = out[row * kSnowAlphaSide + kSnowAlphaSide - 2];
        std::memcpy(out + (kSnowAlphaSide - 1) * kSnowAlphaSide, out + (kSnowAlphaSide - 2) * kSnowAlphaSide,
                    kSnowAlphaSide);
    }
    return packed;
}

uint32_t SnowTexel(float coverage, float slopeFactor, float depthYards, unsigned noise)
{
    return static_cast<uint32_t>(UnitByte(slopeFactor)) << 24 |
           static_cast<uint32_t>(UnitByte(depthYards / kSnowMaxDepthYards)) << 16 |
           static_cast<uint32_t>(UnitByte(coverage)) << 8 | (noise & 0xFFu);
}

float SnowSlopeFactor(float slopeDegrees, const SnowSettings& settings)
{
    if (settings.slopeFade <= 0.0f)
        return slopeDegrees < settings.maxSlope ? 1.0f : 0.0f;
    return std::clamp((settings.maxSlope - slopeDegrees) / settings.slopeFade, 0.0f, 1.0f);
}

float SnowBase(float coverage, float slopeFactor)
{
    const float t = std::clamp((coverage - kDepthCoverageLow) / (kDepthCoverageHigh - kDepthCoverageLow), 0.0f, 1.0f);
    return Smooth(t) * slopeFactor;
}

float SnowDepthAt(float base, float drift, const SnowSettings& settings)
{
    const float lumps = 1.0f + settings.drift * (2.0f * drift - 1.0f);
    return std::clamp(settings.depth * base * lumps, 0.0f, kSnowMaxDepthYards);
}

int SnowTexelIndex(int worldTexel)
{
    const int index = worldTexel % kSnowMapTexels;
    return index < 0 ? index + kSnowMapTexels : index;
}

int SnowBlockStart(int chunk)
{
    return SnowTexelIndex(-(chunk + 1) * kSnowTexelsPerChunk);
}

SnowMap::SnowMap()
    : m_settings(SnowSettingsFrom(Config())), m_texels(static_cast<size_t>(kSnowMapTexels) * kSnowMapTexels, 0),
      m_slots(static_cast<size_t>(kSnowWindowChunks) * kSnowWindowChunks)
{
}

void SnowMap::Configure(const SnowSettings& settings)
{
    if (!SameSnowShape(settings, m_settings))
        for (Slot& slot : m_slots)
            slot.scanned = false;
    m_settings = settings;
}

void SnowMap::Reset()
{
    std::fill(m_texels.begin(), m_texels.end(), 0u);
    std::fill(m_slots.begin(), m_slots.end(), Slot());
    m_placed = false;
    m_mapId = -1;
    m_filled = 0;
    m_snowChunks = 0;
    m_pending = 0;
    ++m_generation;
}

SnowMap::Slot& SnowMap::SlotOf(int row, int col)
{
    const int slotRow = SnowTexelIndex(row * kSnowTexelsPerChunk) / kSnowTexelsPerChunk;
    const int slotCol = SnowTexelIndex(col * kSnowTexelsPerChunk) / kSnowTexelsPerChunk;
    return m_slots[static_cast<size_t>(slotRow) * kSnowWindowChunks + slotCol];
}

void SnowMap::Clear(int row, int col)
{
    Fill(row, col, nullptr);
    Slot& slot = SlotOf(row, col);
    slot = Slot();
    slot.row = row;
    slot.col = col;
    slot.occupied = true;
}

void SnowMap::Fill(int row, int col, const SnowChunk* chunk)
{
    const int u0 = SnowBlockStart(row);
    const int v0 = SnowBlockStart(col);
    const int worldRow = kWorldTexelsFromOrigin - row * kSnowTexelsPerChunk - 1;
    const int worldCol = kWorldTexelsFromOrigin - col * kSnowTexelsPerChunk - 1;
    float slopeFactor[kTerrainChunkCells][kTerrainChunkCells] = {};
    if (chunk)
        for (int a = 0; a < kTerrainChunkCells; ++a)
            for (int b = 0; b < kTerrainChunkCells; ++b)
                slopeFactor[a][b] = SnowSlopeFactor(CellSlopeDegrees(*chunk, a, b), m_settings);
    bool snow = false;
    for (int p = 0; p < kSnowTexelsPerChunk; ++p)
    {
        const float cp = CellCoordinate(p);
        const int a0 = std::min(static_cast<int>(cp), kTerrainChunkCells - 2);
        const float fa = cp - a0;
        for (int q = 0; q < kSnowTexelsPerChunk; ++q)
        {
            const int wi = worldRow - p;
            const int wj = worldCol - q;
            uint32_t texel = 0;
            if (chunk)
            {
                unsigned sum = 0;
                for (int da = 0; da < kSnowAlphaPerTexel; ++da)
                    for (int db = 0; db < kSnowAlphaPerTexel; ++db)
                        sum += chunk->snow[(p * kSnowAlphaPerTexel + da) * kSnowAlphaSide + q * kSnowAlphaPerTexel +
                                           db];
                const float coverage = sum / (kByteScale * kSnowAlphaPerTexel * kSnowAlphaPerTexel);
                const float cq = CellCoordinate(q);
                const int b0 = std::min(static_cast<int>(cq), kTerrainChunkCells - 2);
                const float fb = cq - b0;
                const float top = slopeFactor[a0][b0] + (slopeFactor[a0][b0 + 1] - slopeFactor[a0][b0]) * fb;
                const float bottom =
                    slopeFactor[a0 + 1][b0] + (slopeFactor[a0 + 1][b0 + 1] - slopeFactor[a0 + 1][b0]) * fb;
                const float slope = top + (bottom - top) * fa;
                texel = SnowTexel(coverage, slope, 0.0f, Hash(wi, wj) >> 24);
                snow = snow || coverage > 0.0f;
            }
            const int u = u0 + kSnowTexelsPerChunk - 1 - p;
            const int v = v0 + kSnowTexelsPerChunk - 1 - q;
            m_texels[static_cast<size_t>(v) * kSnowMapTexels + u] = texel;
        }
    }
    Slot& slot = SlotOf(row, col);
    slot.filled = chunk != nullptr;
    slot.snow = snow;
}

bool SnowMap::InWindow(int worldRow, int worldCol) const
{
    const int lowRow = kWorldTexelsFromOrigin - (m_firstRow + kSnowWindowChunks) * kSnowTexelsPerChunk;
    const int lowCol = kWorldTexelsFromOrigin - (m_firstCol + kSnowWindowChunks) * kSnowTexelsPerChunk;
    return worldRow >= lowRow && worldRow < lowRow + kSnowMapTexels && worldCol >= lowCol &&
           worldCol < lowCol + kSnowMapTexels;
}

uint32_t& SnowMap::TexelAt(int worldRow, int worldCol)
{
    return m_texels[static_cast<size_t>(SnowTexelIndex(worldCol)) * kSnowMapTexels + SnowTexelIndex(worldRow)];
}

float SnowMap::BaseAt(int worldRow, int worldCol)
{
    if (!InWindow(worldRow, worldCol))
        return 0.0f;
    const uint32_t texel = TexelAt(worldRow, worldCol);
    return SnowBase(((texel >> kByteShiftCoverage) & 0xFFu) / kByteScale,
                    ((texel >> kByteShiftSlope) & 0xFFu) / kByteScale);
}

void SnowMap::Shape(int row, int col)
{
    const int top = kWorldTexelsFromOrigin - row * kSnowTexelsPerChunk - 1;
    const int left = kWorldTexelsFromOrigin - col * kSnowTexelsPerChunk - 1;
    for (int wi = top - kSnowTexelsPerChunk + 1 - kSnowErosionTexels; wi <= top + kSnowErosionTexels; ++wi)
        for (int wj = left - kSnowTexelsPerChunk + 1 - kSnowErosionTexels; wj <= left + kSnowErosionTexels; ++wj)
        {
            if (!InWindow(wi, wj))
                continue;
            float lowest = 1.0f;
            for (int di = -kSnowErosionTexels; di <= kSnowErosionTexels && lowest > 0.0f; ++di)
                for (int dj = -kSnowErosionTexels; dj <= kSnowErosionTexels; ++dj)
                    lowest = std::min(lowest, BaseAt(wi + di, wj + dj));
            const float depth = SnowDepthAt(lowest, DriftNoise(wi, wj), m_settings);
            uint32_t& texel = TexelAt(wi, wj);
            texel = (texel & ~(0xFFu << kByteShiftDepth)) |
                    static_cast<uint32_t>(UnitByte(depth / kSnowMaxDepthYards)) << kByteShiftDepth;
        }
}

void SnowMap::Place(int cameraRow, int cameraCol, int mapId)
{
    if (mapId != m_mapId)
    {
        Reset();
        m_mapId = mapId;
    }
    const int firstRow = cameraRow - kWindowHalf;
    const int firstCol = cameraCol - kWindowHalf;
    if (m_placed && firstRow == m_firstRow && firstCol == m_firstCol)
        return;
    m_firstRow = firstRow;
    m_firstCol = firstCol;
    m_placed = true;
    ++m_generation;
}

void SnowMap::Count()
{
    m_filled = 0;
    m_snowChunks = 0;
    for (const Slot& slot : m_slots)
    {
        m_filled += slot.filled ? 1 : 0;
        m_snowChunks += slot.snow ? 1 : 0;
    }
}

bool SnowMap::Track(const float* cameraWorld, int mapId, const SnowSource& source, int budget)
{
    Place(TerrainChunkIndex(cameraWorld[0]), TerrainChunkIndex(cameraWorld[1]), mapId);
    const int cameraRow = m_firstRow + kWindowHalf;
    const int cameraCol = m_firstCol + kWindowHalf;
    bool changed = false;
    struct Due
    {
        int row;
        int col;
        int distance;
        uintptr_t identity;
    };
    Due due[kSnowWindowChunks * kSnowWindowChunks];
    Due touched[kSnowWindowChunks * kSnowWindowChunks * 2];
    int dueCount = 0;
    int touchedCount = 0;
    for (int row = m_firstRow; row < m_firstRow + kSnowWindowChunks; ++row)
        for (int col = m_firstCol; col < m_firstCol + kSnowWindowChunks; ++col)
        {
            Slot& slot = SlotOf(row, col);
            if (!slot.occupied || slot.row != row || slot.col != col)
            {
                Clear(row, col);
                touched[touchedCount++] = {row, col, 0, 0};
                changed = true;
            }
            const uintptr_t identity = InMap(row) && InMap(col) ? source.ChunkIdentity(row, col) : 0;
            if (slot.scanned && identity == slot.identity)
                continue;
            if (!identity && !slot.filled)
            {
                slot.identity = 0;
                slot.scanned = true;
                continue;
            }
            due[dueCount++] = {row, col, std::max(std::abs(row - cameraRow), std::abs(col - cameraCol)), identity};
        }
    std::sort(due, due + dueCount, [](const Due& a, const Due& b) { return a.distance < b.distance; });
    const int work = std::min(dueCount, std::max(budget, 0));
    SnowChunk chunk;
    for (int i = 0; i < work; ++i)
    {
        const Due& d = due[i];
        const bool read = d.identity && source.ReadChunk(d.row, d.col, chunk);
        Fill(d.row, d.col, read ? &chunk : nullptr);
        Slot& slot = SlotOf(d.row, d.col);
        slot.identity = d.identity;
        slot.scanned = true;
        touched[touchedCount++] = d;
        changed = true;
    }
    for (int i = 0; i < touchedCount; ++i)
        Shape(touched[i].row, touched[i].col);
    m_pending = dueCount - work;
    Count();
    if (changed)
        ++m_generation;
    return changed;
}

void SnowMap::Box(float* box) const
{
    box[0] = kTerrainMapOriginYards - (m_firstRow + kSnowWindowChunks) * kTerrainChunkYards;
    box[1] = kTerrainMapOriginYards - (m_firstCol + kSnowWindowChunks) * kTerrainChunkYards;
    box[2] = kTerrainMapOriginYards - m_firstRow * kTerrainChunkYards;
    box[3] = kTerrainMapOriginYards - m_firstCol * kTerrainChunkYards;
}

SnowVertexConstants SnowVertexConstantsFor(const SnowMap& map, bool active)
{
    SnowVertexConstants out = {};
    out.registers[0][0] = 1.0f / kSnowMapYards;
    out.registers[0][2] = kSnowMaxDepthYards;
    map.Box(out.registers[1]);
    out.registers[2][0] = 1.0f / kSnowEdgeFadeYards;
    out.registers[2][1] = active && map.Placed() && map.Settings().enabled ? 1.0f : 0.0f;
    return out;
}

SnowPixelConstants SnowPixelConstantsFor(const SnowMap& map, bool active)
{
    const SnowSettings& s = map.Settings();
    SnowPixelConstants out = {};
    out.registers[0][0] = 1.0f / kSnowMapYards;
    out.registers[0][1] = kSnowDetailScale;
    out.registers[0][2] = s.breakup;
    out.registers[0][3] = active && map.Placed() && s.enabled ? s.edges : 0.0f;
    map.Box(out.registers[1]);
    out.registers[2][0] = 1.0f / kSnowEdgeFadeYards;
    out.registers[2][2] = 2.0f * kSnowAlbedo;
    out.registers[2][3] = kSnowShadowGain;
    out.registers[3][0] = kSnowEdgeSharpness;
    out.registers[3][1] = 0.5f - 0.5f * kSnowEdgeSharpness;
    return out;
}

SnowMap& GlobalSnowMap()
{
    static SnowMap map;
    return map;
}

SnowFrame& GlobalSnowFrame()
{
    static SnowFrame frame = {};
    return frame;
}

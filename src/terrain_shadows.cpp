#include "terrain_shadows.h"

#include "d3d_util.h"
#include "log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
constexpr int kWindowChunkSlots = kTerrainWindowChunks * kTerrainWindowChunks;
constexpr int kWindowVertexCount = kTerrainWindowVertices * kTerrainWindowVertices;
constexpr float kMinHorizontalLight = 1e-6f;
constexpr float kLightRecomputeCosine = 0.999998f;
constexpr unsigned char kNotLoaded = 0;
constexpr unsigned char kLoaded = 1;
constexpr uintptr_t kUnscannedIdentity = ~static_cast<uintptr_t>(0);

bool InMap(int chunk)
{
    return chunk >= 0 && chunk < kTerrainMapChunks;
}

float UpstreamShadowHeight(const float* shadowHeights, int line, int majorStride, int minorStride, float minor)
{
    const float cell = std::floor(minor);
    const int first = static_cast<int>(cell);
    const float fraction = minor - cell;
    if (first < 0 || first >= kTerrainWindowVertices)
        return kNoTerrainHeight;
    const float lower = shadowHeights[line * majorStride + first * minorStride];
    if (first + 1 >= kTerrainWindowVertices)
        return fraction > 0.0f ? kNoTerrainHeight : lower;
    const float upper = shadowHeights[line * majorStride + (first + 1) * minorStride];
    return lower + (upper - lower) * fraction;
}
}

int TerrainChunkIndex(float worldYards)
{
    return static_cast<int>(std::floor((kTerrainMapOriginYards - worldYards) / kTerrainChunkYards));
}

float TerrainVertexCoordinate(float worldYards)
{
    return (kTerrainMapOriginYards - worldYards) / kTerrainVertexYards;
}

TerrainHeightWindow::TerrainHeightWindow()
    : m_heights(kWindowVertexCount, kNoTerrainHeight), m_identities(kWindowChunkSlots, 0),
      m_loaded(kWindowChunkSlots, kNotLoaded)
{
}

void TerrainHeightWindow::Reset()
{
    std::fill(m_heights.begin(), m_heights.end(), kNoTerrainHeight);
    std::fill(m_identities.begin(), m_identities.end(), 0);
    std::fill(m_loaded.begin(), m_loaded.end(), kNotLoaded);
    m_loadedChunks = 0;
    ++m_generation;
    m_scanPhase = 0;
    m_scanAll = true;
    m_placed = false;
    m_mapId = -1;
}

bool TerrainHeightWindow::Recentre(int cameraChunkRow, int cameraChunkCol, int mapId)
{
    const int half = kTerrainWindowChunks / 2;
    const bool keeps = m_placed && mapId == m_mapId &&
                       std::abs(cameraChunkRow - (m_firstChunkRow + half)) <= kTerrainWindowRecentreChunks &&
                       std::abs(cameraChunkCol - (m_firstChunkCol + half)) <= kTerrainWindowRecentreChunks;
    if (keeps)
        return false;
    const int firstChunkRow = cameraChunkRow - half;
    const int firstChunkCol = cameraChunkCol - half;
    const bool overlaps = m_placed && mapId == m_mapId &&
                          std::abs(firstChunkRow - m_firstChunkRow) < kTerrainWindowChunks &&
                          std::abs(firstChunkCol - m_firstChunkCol) < kTerrainWindowChunks;
    const bool hadTerrain = m_loadedChunks > 0;
    if (overlaps)
    {
        ShiftTo(firstChunkRow, firstChunkCol);
    }
    else
    {
        Reset();
        m_firstChunkRow = firstChunkRow;
        m_firstChunkCol = firstChunkCol;
    }
    m_mapId = mapId;
    m_placed = true;
    m_scanAll = true;
    ++m_generation;
    return hadTerrain;
}

void TerrainHeightWindow::ShiftTo(int firstChunkRow, int firstChunkCol)
{
    const int rowShift = firstChunkRow - m_firstChunkRow;
    const int colShift = firstChunkCol - m_firstChunkCol;
    const int firstCol = std::max(0, -colShift);
    const int endCol = std::min(kTerrainWindowChunks, kTerrainWindowChunks - colShift);
    m_shiftedHeights.assign(m_heights.size(), kNoTerrainHeight);
    m_shiftedIdentities.assign(m_identities.size(), kUnscannedIdentity);
    m_shiftedLoaded.assign(m_loaded.size(), kNotLoaded);
    int loadedChunks = 0;
    for (int row = 0; row < kTerrainWindowChunks; ++row)
    {
        const int oldRow = row + rowShift;
        if (oldRow < 0 || oldRow >= kTerrainWindowChunks)
            continue;
        for (int col = firstCol; col < endCol; ++col)
        {
            const int slot = row * kTerrainWindowChunks + col;
            const int oldSlot = oldRow * kTerrainWindowChunks + col + colShift;
            m_shiftedIdentities[slot] = m_identities[oldSlot];
            m_shiftedLoaded[slot] = m_loaded[oldSlot];
            loadedChunks += m_loaded[oldSlot];
        }
        for (int line = 0; line < kTerrainChunkCells; ++line)
            std::memcpy(&m_shiftedHeights[(row * kTerrainChunkCells + line) * kTerrainWindowVertices +
                                          firstCol * kTerrainChunkCells],
                        &m_heights[(oldRow * kTerrainChunkCells + line) * kTerrainWindowVertices +
                                   (firstCol + colShift) * kTerrainChunkCells],
                        sizeof(float) * (endCol - firstCol) * kTerrainChunkCells);
    }
    m_heights.swap(m_shiftedHeights);
    m_identities.swap(m_shiftedIdentities);
    m_loaded.swap(m_shiftedLoaded);
    m_firstChunkRow = firstChunkRow;
    m_firstChunkCol = firstChunkCol;
    m_loadedChunks = loadedChunks;
}

void TerrainHeightWindow::WriteChunk(int windowRow, int windowCol, const TerrainChunkHeights* chunk)
{
    for (int row = 0; row < kTerrainChunkCells; ++row)
    {
        float* line = &m_heights[(windowRow * kTerrainChunkCells + row) * kTerrainWindowVertices +
                                 windowCol * kTerrainChunkCells];
        for (int col = 0; col < kTerrainChunkCells; ++col)
            line[col] = chunk ? chunk->heights[row][col] : kNoTerrainHeight;
    }
}

bool TerrainHeightWindow::RefreshChunk(int windowRow, int windowCol, const TerrainSource& source)
{
    const int chunkRow = m_firstChunkRow + windowRow;
    const int chunkCol = m_firstChunkCol + windowCol;
    const int slot = windowRow * kTerrainWindowChunks + windowCol;
    const uintptr_t identity = InMap(chunkRow) && InMap(chunkCol) ? source.ChunkIdentity(chunkRow, chunkCol) : 0;
    if (identity == m_identities[slot])
        return false;
    TerrainChunkHeights chunk;
    const bool loaded = identity != 0 && source.ReadChunk(chunkRow, chunkCol, chunk);
    const bool wasLoaded = m_loaded[slot] != 0;
    m_identities[slot] = identity;
    m_loaded[slot] = loaded ? kLoaded : kNotLoaded;
    m_loadedChunks += (loaded ? 1 : 0) - (wasLoaded ? 1 : 0);
    if (!loaded && !wasLoaded)
        return false;
    WriteChunk(windowRow, windowCol, loaded ? &chunk : nullptr);
    return true;
}

bool TerrainHeightWindow::Track(const float* cameraWorld, int mapId, const TerrainSource& source)
{
    bool changed = Recentre(TerrainChunkIndex(cameraWorld[0]), TerrainChunkIndex(cameraWorld[1]), mapId);
    const int stride = m_scanAll ? 1 : kTerrainScanPhases;
    for (int slot = m_scanAll ? 0 : m_scanPhase; slot < kWindowChunkSlots; slot += stride)
        changed |= RefreshChunk(slot / kTerrainWindowChunks, slot % kTerrainWindowChunks, source);
    m_scanAll = false;
    m_scanPhase = (m_scanPhase + 1) % kTerrainScanPhases;
    return changed;
}

namespace
{
constexpr int kTransposeBlock = 16;

struct Sweep
{
    int step;
    int cell;
    float fraction;
    float slope;
    float drop;
};

void Transpose(const float* from, float* to)
{
    for (int blockRow = 0; blockRow < kTerrainWindowVertices; blockRow += kTransposeBlock)
        for (int blockCol = 0; blockCol < kTerrainWindowVertices; blockCol += kTransposeBlock)
            for (int row = blockRow; row < blockRow + kTransposeBlock; ++row)
                for (int col = blockCol; col < blockCol + kTransposeBlock; ++col)
                    to[col * kTerrainWindowVertices + row] = from[row * kTerrainWindowVertices + col];
}

float EdgeShadowHeight(float ground, const float* shadowHeights, int upstream, int n, const Sweep& sweep)
{
    return std::max(ground, UpstreamShadowHeight(shadowHeights, upstream, kTerrainWindowVertices, 1,
                                                 static_cast<float>(n) + sweep.slope) -
                                sweep.drop);
}

void SweepLines(const float* heights, float* shadowHeights, const Sweep& sweep)
{
    constexpr int side = kTerrainWindowVertices;
    const int firstInterior = std::clamp(-sweep.cell, 0, side);
    const int endInterior = std::clamp(side - 1 - sweep.cell, firstInterior, side);
    for (int k = 0; k < side; ++k)
    {
        const int line = sweep.step > 0 ? side - 1 - k : k;
        const int upstream = line + sweep.step;
        float* __restrict row = shadowHeights + line * side;
        const float* __restrict ground = heights + line * side;
        if (upstream < 0 || upstream >= side)
        {
            std::memcpy(row, ground, sizeof(float) * side);
            continue;
        }
        const float* __restrict previous = shadowHeights + upstream * side;
        for (int n = 0; n < firstInterior; ++n)
            row[n] = EdgeShadowHeight(ground[n], shadowHeights, upstream, n, sweep);
        for (int n = firstInterior; n < endInterior; ++n)
        {
            const float lower = previous[n + sweep.cell];
            const float upper = previous[n + sweep.cell + 1];
            row[n] = std::max(ground[n], lower + (upper - lower) * sweep.fraction - sweep.drop);
        }
        for (int n = endInterior; n < side; ++n)
            row[n] = EdgeShadowHeight(ground[n], shadowHeights, upstream, n, sweep);
    }
}
}

void ComputeTerrainShadowHeights(const float* heights, const float* toLight, float* shadowHeights)
{
    const float towardRows = -toLight[0];
    const float towardCols = -toLight[1];
    const float horizontal = std::sqrt(towardRows * towardRows + towardCols * towardCols);
    if (horizontal < kMinHorizontalLight)
    {
        std::memcpy(shadowHeights, heights, sizeof(float) * kWindowVertexCount);
        return;
    }
    const bool rowsMajor = std::fabs(towardRows) >= std::fabs(towardCols);
    const float major = rowsMajor ? towardRows : towardCols;
    const float slope = (rowsMajor ? towardCols : towardRows) / std::fabs(major);
    const float cell = std::floor(slope);
    const Sweep sweep = {major > 0.0f ? 1 : -1, static_cast<int>(cell), slope - cell, slope,
                         kTerrainVertexYards * std::sqrt(1.0f + slope * slope) * toLight[2] / horizontal};
    if (rowsMajor)
    {
        SweepLines(heights, shadowHeights, sweep);
        return;
    }
    thread_local std::vector<float> scratch;
    scratch.resize(static_cast<size_t>(kWindowVertexCount) * 2);
    float* transposedHeights = scratch.data();
    float* transposedShadow = transposedHeights + kWindowVertexCount;
    Transpose(heights, transposedHeights);
    SweepLines(transposedHeights, transposedShadow, sweep);
    Transpose(transposedShadow, shadowHeights);
}

bool TerrainShadowSchedule::Due(bool structural, bool terrainChanged, bool lightMoved, bool computed, double seconds)
{
    if (terrainChanged)
        m_lastTerrainChange = seconds;
    if (!computed || structural)
        return true;
    if ((terrainChanged || lightMoved) && !m_pending)
    {
        m_pending = true;
        m_pendingSince = seconds;
    }
    return m_pending && (seconds - m_lastTerrainChange >= kTerrainSettleSeconds ||
                         seconds - m_pendingSince >= kTerrainMaxDelaySeconds);
}

TerrainShadowMap::~TerrainShadowMap()
{
    ReleaseTexture();
}

void TerrainShadowMap::ReleaseTexture()
{
    SafeRelease(m_texture);
    m_current = false;
}

bool TerrainShadowMap::LightMoved(const float* toLight) const
{
    return toLight[0] * m_toLight[0] + toLight[1] * m_toLight[1] + toLight[2] * m_toLight[2] <
           kLightRecomputeCosine;
}

bool TerrainShadowMap::Upload()
{
    D3DLOCKED_RECT locked = {};
    if (FAILED(m_texture->LockRect(0, &locked, nullptr, 0)))
        return false;
    for (int row = 0; row < kTerrainWindowVertices; ++row)
        std::memcpy(static_cast<unsigned char*>(locked.pBits) + row * locked.Pitch,
                    &m_shadowHeights[row * kTerrainWindowVertices], sizeof(float) * kTerrainWindowVertices);
    return SUCCEEDED(m_texture->UnlockRect(0));
}

bool TerrainShadowMap::Update(IDirect3DDevice9* dev, const float* cameraWorld, int mapId, const float* toLight,
                              const TerrainSource& source, double seconds)
{
    const bool terrainChanged = m_window.Track(cameraWorld, mapId, source);
    if (m_window.LoadedChunks() == 0)
        return false;
    if (!m_texture && !m_creationFailed)
    {
        if (FAILED(dev->CreateTexture(kTerrainWindowVertices, kTerrainWindowVertices, 1, 0, D3DFMT_R32F,
                                      D3DPOOL_MANAGED, &m_texture, nullptr)))
        {
            m_texture = nullptr;
            m_creationFailed = true;
            VF_LOG_ERROR("terrain shadows: %dx%d R32F texture unavailable; terrain does not block the sun",
                         kTerrainWindowVertices, kTerrainWindowVertices);
        }
        m_current = false;
    }
    if (!m_texture)
        return false;
    const bool structural = m_window.Generation() != m_computedGeneration;
    if (!m_schedule.Due(structural, terrainChanged, LightMoved(toLight), m_current, seconds))
        return true;
    m_shadowHeights.resize(static_cast<size_t>(kWindowVertexCount));
    ComputeTerrainShadowHeights(m_window.Heights(), toLight, m_shadowHeights.data());
    std::memcpy(m_toLight, toLight, sizeof(m_toLight));
    m_computedGeneration = m_window.Generation();
    m_schedule.Computed();
    ++m_recomputes;
    m_current = Upload();
    return m_current;
}

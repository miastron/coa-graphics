#pragma once

#include <d3d9.h>

#include <cstdint>
#include <vector>

constexpr int kTerrainChunkVertexSide = 9;
constexpr int kTerrainChunkCells = 8;
constexpr int kTerrainMapChunks = 1024;
constexpr int kTerrainWindowChunks = 64;
constexpr int kTerrainWindowVertices = kTerrainWindowChunks * kTerrainChunkCells;
constexpr int kTerrainWindowRecentreChunks = 8;
constexpr int kTerrainScanPhases = 8;
constexpr double kTerrainSettleSeconds = 0.25;
constexpr double kTerrainMaxDelaySeconds = 1.0;
constexpr float kTerrainMapOriginYards = 17066.666f;
constexpr float kTerrainChunkYards = 33.333332f;
constexpr float kTerrainVertexYards = kTerrainChunkYards / kTerrainChunkCells;
constexpr float kNoTerrainHeight = -100000.0f;

struct TerrainChunkHeights
{
    float heights[kTerrainChunkVertexSide][kTerrainChunkVertexSide];
};

class TerrainSource
{
public:
    virtual uintptr_t ChunkIdentity(int chunkRow, int chunkCol) const = 0;
    virtual bool ReadChunk(int chunkRow, int chunkCol, TerrainChunkHeights& out) const = 0;

protected:
    ~TerrainSource() = default;
};

int TerrainChunkIndex(float worldYards);
float TerrainVertexCoordinate(float worldYards);

class TerrainHeightWindow
{
public:
    TerrainHeightWindow();

    bool Track(const float* cameraWorld, int mapId, const TerrainSource& source);
    void Reset();
    const float* Heights() const { return m_heights.data(); }
    int FirstVertexRow() const { return m_firstChunkRow * kTerrainChunkCells; }
    int FirstVertexCol() const { return m_firstChunkCol * kTerrainChunkCells; }
    int LoadedChunks() const { return m_loadedChunks; }
    unsigned Generation() const { return m_generation; }

private:
    bool Recentre(int cameraChunkRow, int cameraChunkCol, int mapId);
    void ShiftTo(int firstChunkRow, int firstChunkCol);
    bool RefreshChunk(int windowRow, int windowCol, const TerrainSource& source);
    void WriteChunk(int windowRow, int windowCol, const TerrainChunkHeights* chunk);

    std::vector<float> m_heights;
    std::vector<float> m_shiftedHeights;
    std::vector<uintptr_t> m_identities;
    std::vector<uintptr_t> m_shiftedIdentities;
    std::vector<unsigned char> m_loaded;
    std::vector<unsigned char> m_shiftedLoaded;
    int m_firstChunkRow = 0;
    int m_firstChunkCol = 0;
    int m_mapId = -1;
    int m_loadedChunks = 0;
    int m_scanPhase = 0;
    unsigned m_generation = 0;
    bool m_scanAll = true;
    bool m_placed = false;
};

void ComputeTerrainShadowHeights(const float* heights, const float* toLight, float* shadowHeights);

class TerrainShadowSchedule
{
public:
    bool Due(bool structural, bool terrainChanged, bool lightMoved, bool computed, double seconds);
    void Computed() { m_pending = false; }

private:
    double m_pendingSince = 0.0;
    double m_lastTerrainChange = -kTerrainMaxDelaySeconds;
    bool m_pending = false;
};

class TerrainShadowMap
{
public:
    ~TerrainShadowMap();

    bool Update(IDirect3DDevice9* dev, const float* cameraWorld, int mapId, const float* toLight,
                const TerrainSource& source, double seconds);
    void ReleaseTexture();
    IDirect3DTexture9* Texture() const { return m_texture; }
    const TerrainHeightWindow& Window() const { return m_window; }
    unsigned Recomputes() const { return m_recomputes; }

private:
    bool LightMoved(const float* toLight) const;
    bool Upload();

    TerrainHeightWindow m_window;
    TerrainShadowSchedule m_schedule;
    std::vector<float> m_shadowHeights;
    IDirect3DTexture9* m_texture = nullptr;
    float m_toLight[3] = {};
    unsigned m_computedGeneration = 0;
    unsigned m_recomputes = 0;
    bool m_current = false;
    bool m_creationFailed = false;
};

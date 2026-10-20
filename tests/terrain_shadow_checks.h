#pragma once

namespace terrain_shadow_checks
{
constexpr int kMap = 0;
constexpr int kOtherMap = 1;
constexpr int kCameraChunk = 500;
constexpr float kWallHeight = 100.0f;
constexpr float kShadowTolerance = 0.01f;
constexpr int kWallVertex = 300;
constexpr float kLightElevationDegrees = 10.0f;

using VertexHeight = std::function<float(int vertexRow, int vertexCol)>;

class SyntheticTerrain final : public TerrainSource
{
public:
    explicit SyntheticTerrain(VertexHeight height) : m_height(std::move(height)) {}

    uintptr_t ChunkIdentity(int chunkRow, int chunkCol) const override
    {
        ++identityCalls;
        if (Unloaded(chunkRow, chunkCol))
            return 0;
        return static_cast<uintptr_t>(chunkRow * kTerrainMapChunks + chunkCol + 1) * 16 + m_generation;
    }

    bool ReadChunk(int chunkRow, int chunkCol, TerrainChunkHeights& out) const override
    {
        ++readCalls;
        for (int row = 0; row < kTerrainChunkVertexSide; ++row)
            for (int col = 0; col < kTerrainChunkVertexSide; ++col)
                out.heights[row][col] =
                    m_height(chunkRow * kTerrainChunkCells + row, chunkCol * kTerrainChunkCells + col);
        return true;
    }

    void ResetCounts()
    {
        identityCalls = 0;
        readCalls = 0;
    }

    void Unload(int chunkRow, int chunkCol)
    {
        m_unloadedRow = chunkRow;
        m_unloadedCol = chunkCol;
    }

    void Regenerate(VertexHeight height)
    {
        m_height = std::move(height);
        ++m_generation;
    }

    mutable int identityCalls = 0;
    mutable int readCalls = 0;

private:
    bool Unloaded(int chunkRow, int chunkCol) const
    {
        return chunkRow == m_unloadedRow && chunkCol == m_unloadedCol;
    }

    VertexHeight m_height;
    int m_unloadedRow = -1;
    int m_unloadedCol = -1;
    uintptr_t m_generation = 0;
};

float ChunkCentreYards(int chunk)
{
    return kTerrainMapOriginYards - (chunk + 0.5f) * kTerrainChunkYards;
}

void CameraOverChunk(int chunkRow, int chunkCol, float* camera)
{
    camera[0] = ChunkCentreYards(chunkRow);
    camera[1] = ChunkCentreYards(chunkCol);
    camera[2] = 0.0f;
}

float WindowHeight(const TerrainHeightWindow& window, int vertexRow, int vertexCol)
{
    return window.Heights()[(vertexRow - window.FirstVertexRow()) * kTerrainWindowVertices + vertexCol -
                            window.FirstVertexCol()];
}

float MapVertexHeight(int vertexRow, int vertexCol)
{
    return static_cast<float>(vertexRow % 97) + static_cast<float>(vertexCol % 89) * 0.01f;
}

bool TrackThroughScan(TerrainHeightWindow& window, const float* camera, int mapId, const TerrainSource& source)
{
    bool changed = false;
    for (int frame = 0; frame < kTerrainScanPhases; ++frame)
        changed |= window.Track(camera, mapId, source);
    return changed;
}

bool WindowMatchesMap(const TerrainHeightWindow& window)
{
    for (int row = 0; row < kTerrainWindowVertices; ++row)
        for (int col = 0; col < kTerrainWindowVertices; ++col)
            if (WindowHeight(window, window.FirstVertexRow() + row, window.FirstVertexCol() + col) !=
                MapVertexHeight(window.FirstVertexRow() + row, window.FirstVertexCol() + col))
                return false;
    return true;
}

void CheckWindowPlacesClientChunks()
{
    SyntheticTerrain terrain(MapVertexHeight);
    TerrainHeightWindow window;
    float camera[3];
    CameraOverChunk(kCameraChunk, kCameraChunk + 3, camera);
    Check(window.Track(camera, kMap, terrain) && window.LoadedChunks() == kTerrainWindowChunks * kTerrainWindowChunks,
          "terrain window reads every chunk around the camera");
    Check(window.FirstVertexRow() == (kCameraChunk - kTerrainWindowChunks / 2) * kTerrainChunkCells &&
              window.FirstVertexCol() == (kCameraChunk + 3 - kTerrainWindowChunks / 2) * kTerrainChunkCells,
          "terrain window is centred on the camera's chunk, rows along world -X and columns along world -Y");
    const int vertexRow = kCameraChunk * kTerrainChunkCells + 5;
    const int vertexCol = (kCameraChunk + 3) * kTerrainChunkCells + 2;
    Check(WindowHeight(window, vertexRow, vertexCol) == MapVertexHeight(vertexRow, vertexCol),
          "terrain window stores a chunk's outer vertex at its map vertex");
    const float vertexX = kTerrainMapOriginYards - vertexRow * kTerrainVertexYards;
    const float vertexY = kTerrainMapOriginYards - vertexCol * kTerrainVertexYards;
    Check(std::fabs(TerrainVertexCoordinate(vertexX) - vertexRow) < 1e-2f &&
              std::fabs(TerrainVertexCoordinate(vertexY) - vertexCol) < 1e-2f,
          "terrain vertex coordinates invert the client's chunk vertex positions");
    Check(!window.Track(camera, kMap, terrain), "an unchanged terrain window reports no change");
}

void CheckWindowFollowsLoadingAndMaps()
{
    SyntheticTerrain terrain(MapVertexHeight);
    TerrainHeightWindow window;
    float camera[3];
    CameraOverChunk(kCameraChunk, kCameraChunk, camera);
    window.Track(camera, kMap, terrain);
    terrain.Unload(kCameraChunk, kCameraChunk);
    const int vertex = kCameraChunk * kTerrainChunkCells;
    Check(TrackThroughScan(window, camera, kMap, terrain) &&
              WindowHeight(window, vertex, vertex) == kNoTerrainHeight &&
              window.LoadedChunks() == kTerrainWindowChunks * kTerrainWindowChunks - 1,
          "an unloaded chunk leaves no terrain in the window within one scan cycle");
    terrain.Unload(-1, -1);
    TrackThroughScan(window, camera, kMap, terrain);
    const int firstRow = window.FirstVertexRow();
    CameraOverChunk(kCameraChunk + kTerrainWindowRecentreChunks, kCameraChunk, camera);
    window.Track(camera, kMap, terrain);
    Check(window.FirstVertexRow() == firstRow, "terrain window keeps its place while the camera stays near its centre");
    CameraOverChunk(kCameraChunk + kTerrainWindowRecentreChunks + 1, kCameraChunk, camera);
    window.Track(camera, kMap, terrain);
    Check(window.FirstVertexRow() == firstRow + (kTerrainWindowRecentreChunks + 1) * kTerrainChunkCells,
          "terrain window recentres once the camera leaves its centre");
    terrain.Regenerate([](int, int) { return 7.0f; });
    Check(window.Track(camera, kOtherMap, terrain) && WindowHeight(window, vertex, vertex) == 7.0f,
          "a map change rereads the terrain window");
}

void CheckSteadyScanIsStaggered()
{
    SyntheticTerrain terrain(MapVertexHeight);
    TerrainHeightWindow window;
    float camera[3];
    CameraOverChunk(kCameraChunk, kCameraChunk, camera);
    window.Track(camera, kMap, terrain);
    Check(terrain.identityCalls == kTerrainWindowChunks * kTerrainWindowChunks,
          "the first terrain scan checks every chunk of the window");
    terrain.ResetCounts();
    window.Track(camera, kMap, terrain);
    Check(terrain.identityCalls == kTerrainWindowChunks * kTerrainWindowChunks / kTerrainScanPhases &&
              terrain.readCalls == 0,
          "a steady frame checks only its share of the window's chunks and reads none");
    terrain.ResetCounts();
    TrackThroughScan(window, camera, kMap, terrain);
    Check(terrain.identityCalls == kTerrainWindowChunks * kTerrainWindowChunks,
          "every chunk of the window is checked once per scan cycle");
}

void CheckRecentreKeepsOverlap()
{
    SyntheticTerrain terrain(MapVertexHeight);
    TerrainHeightWindow window;
    float camera[3];
    CameraOverChunk(kCameraChunk, kCameraChunk, camera);
    window.Track(camera, kMap, terrain);
    const int unloadedRow = kCameraChunk - kTerrainWindowChunks / 2 + 20;
    const int unloadedCol = kCameraChunk - kTerrainWindowChunks / 2 + 30;
    terrain.Unload(unloadedRow, unloadedCol);
    TrackThroughScan(window, camera, kMap, terrain);
    terrain.ResetCounts();
    constexpr int kRowStep = kTerrainWindowRecentreChunks + 1;
    constexpr int kColStep = -3;
    CameraOverChunk(kCameraChunk + kRowStep, kCameraChunk + kColStep, camera);
    const bool changed = window.Track(camera, kMap, terrain);
    constexpr int kNewChunks = kTerrainWindowChunks * kTerrainWindowChunks -
                               (kTerrainWindowChunks - kRowStep) * (kTerrainWindowChunks + kColStep);
    Check(changed && terrain.readCalls == kNewChunks,
          "a recentre reads only the chunks that entered the window");
    const int vertexRow = unloadedRow * kTerrainChunkCells;
    const int vertexCol = unloadedCol * kTerrainChunkCells;
    Check(window.LoadedChunks() == kTerrainWindowChunks * kTerrainWindowChunks - 1 &&
              WindowHeight(window, vertexRow, vertexCol) == kNoTerrainHeight,
          "a recentre keeps an unloaded chunk unloaded");
    terrain.Unload(-1, -1);
    TrackThroughScan(window, camera, kMap, terrain);
    Check(WindowMatchesMap(window) && window.LoadedChunks() == kTerrainWindowChunks * kTerrainWindowChunks,
          "a recentre keeps every overlapping chunk at its map position");
    terrain.ResetCounts();
    CameraOverChunk(kCameraChunk + kRowStep + kTerrainWindowChunks, kCameraChunk + kColStep, camera);
    window.Track(camera, kMap, terrain);
    Check(terrain.readCalls == kTerrainWindowChunks * kTerrainWindowChunks && WindowMatchesMap(window),
          "a recentre past the whole window rereads it");
}

void ToLightAt(float towardRows, float towardCols, float* toLight)
{
    const float elevation = kLightElevationDegrees * 3.14159265f / 180.0f;
    const float horizontal = std::sqrt(towardRows * towardRows + towardCols * towardCols);
    toLight[0] = -towardRows / horizontal * std::cos(elevation);
    toLight[1] = -towardCols / horizontal * std::cos(elevation);
    toLight[2] = std::sin(elevation);
}

std::vector<float> Wall(bool alongCols)
{
    std::vector<float> heights(kTerrainWindowVertices * kTerrainWindowVertices, 0.0f);
    for (int n = 0; n < kTerrainWindowVertices; ++n)
        heights[alongCols ? kWallVertex * kTerrainWindowVertices + n : n * kTerrainWindowVertices + kWallVertex] =
            kWallHeight;
    return heights;
}

void CheckWallShadow(bool alongCols, float towardRows, float towardCols, const char* name)
{
    const std::vector<float> heights = Wall(alongCols);
    std::vector<float> shadow(heights.size());
    float toLight[3];
    ToLightAt(towardRows, towardCols, toLight);
    ComputeTerrainShadowHeights(heights.data(), toLight, shadow.data());
    const float tangent = std::tan(kLightElevationDegrees * 3.14159265f / 180.0f);
    const float acrossWall = alongCols ? towardRows : towardCols;
    const float horizontalPerAcross =
        std::sqrt(towardRows * towardRows + towardCols * towardCols) / std::fabs(acrossWall);
    const int away = acrossWall > 0 ? -1 : 1;
    bool behind = true;
    for (int k = 1; k < 40; ++k)
    {
        const int across = kWallVertex + away * k;
        const int index = alongCols ? across * kTerrainWindowVertices + 200 : 200 * kTerrainWindowVertices + across;
        const float expected = kWallHeight - k * kTerrainVertexYards * horizontalPerAcross * tangent;
        behind = behind && std::fabs(shadow[index] - std::max(expected, 0.0f)) <= kShadowTolerance;
    }
    const int lit = alongCols ? (kWallVertex - away * 5) * kTerrainWindowVertices + 200
                              : 200 * kTerrainWindowVertices + kWallVertex - away * 5;
    char label[200];
    std::snprintf(label, sizeof(label), "terrain shadow height behind a wall, %s, falls with the light's slope", name);
    Check(behind, label);
    std::snprintf(label, sizeof(label), "terrain shadow height on the light's side of a wall, %s, stays the ground",
                  name);
    Check(shadow[lit] == 0.0f, label);
}

void CheckShadowHeights()
{
    CheckWallShadow(true, 1.0f, 0.0f, "light along rows");
    CheckWallShadow(true, -1.0f, 0.0f, "light against rows");
    CheckWallShadow(false, 0.0f, 1.0f, "light along columns");
    CheckWallShadow(true, 1.0f, 1.0f, "diagonal light");
    CheckWallShadow(false, -0.5f, -1.0f, "oblique light");
    std::vector<float> heights = Wall(true);
    std::vector<float> shadow(heights.size());
    const float overhead[3] = {0.0f, 0.0f, 1.0f};
    ComputeTerrainShadowHeights(heights.data(), overhead, shadow.data());
    Check(shadow == heights, "a light overhead leaves the terrain its own shadow height");
}

constexpr float kSweepTolerance = 0.05f;
constexpr double kFrameSeconds = 1.0 / 60.0;
constexpr int kStreamingFrames = 300;

struct LightCase
{
    float towardRows;
    float towardCols;
    float elevationDegrees;
};

constexpr LightCase kLightCases[] = {
    {1.0f, 0.0f, 10.0f},   {-1.0f, 0.0f, 10.0f},  {0.0f, 1.0f, 10.0f},   {0.0f, -1.0f, 10.0f},
    {1.0f, 1.0f, 25.0f},   {-1.0f, 1.0f, 5.0f},   {1.0f, -1.0f, 40.0f},  {-0.5f, -1.0f, 10.0f},
    {0.3f, 1.0f, 15.0f},   {1.0f, 0.999f, 10.0f}, {1.0f, -0.999f, 10.0f}, {1.0f, 0.001f, 10.0f},
    {-0.37f, 1.0f, 33.0f}, {1.0f, 0.0f, -5.0f},   {0.0f, -1.0f, -5.0f},
};

std::vector<float> RoughTerrain(unsigned seed)
{
    std::vector<float> heights(static_cast<size_t>(kTerrainWindowVertices) * kTerrainWindowVertices);
    unsigned state = seed;
    for (float& height : heights)
    {
        state = state * 1664525u + 1013904223u;
        const unsigned bits = state >> 8;
        height = static_cast<float>(bits % 4000) * 0.05f;
        if (bits % 97 == 0)
            height = kNoTerrainHeight;
    }
    return heights;
}

void CheckSweepMatchesReference()
{
    const std::vector<float> heights = RoughTerrain(12345u);
    std::vector<float> exact(heights.size());
    std::vector<float> reference(heights.size());
    std::vector<float> swept(heights.size());
    float worst = 0.0f;
    float referenceWorst = 0.0f;
    bool sentinelsAgree = true;
    for (const LightCase& light : kLightCases)
    {
        float toLight[3];
        ToLightAt(light.towardRows, light.towardCols, toLight);
        toLight[2] = std::sin(light.elevationDegrees * 3.14159265f / 180.0f);
        terrain_shadow_reference::ExactShadowHeights(heights.data(), toLight, exact.data());
        terrain_shadow_reference::ShadowHeights(heights.data(), toLight, reference.data());
        ComputeTerrainShadowHeights(heights.data(), toLight, swept.data());
        for (size_t i = 0; i < heights.size(); ++i)
        {
            const bool exactHole = exact[i] <= kNoTerrainHeight / 2;
            sentinelsAgree = sentinelsAgree && exactHole == (swept[i] <= kNoTerrainHeight / 2);
            if (!exactHole && swept[i] > kNoTerrainHeight / 2)
            {
                worst = std::max(worst, std::fabs(exact[i] - swept[i]));
                referenceWorst = std::max(referenceWorst, std::fabs(exact[i] - reference[i]));
            }
        }
    }
    std::printf("     terrain shadow sweep vs a double-precision sweep over %zu light directions: worst difference "
                "%.5f yd, the per-vertex float sweep it replaces %.5f yd\n",
                std::size(kLightCases), worst, referenceWorst);
    Check(worst <= kSweepTolerance && sentinelsAgree,
          "the terrain shadow sweep matches an exact sweep for every light direction, slope and edge");
}

void CheckSweepOnAWallStillShadows()
{
    const std::vector<float> heights = Wall(true);
    std::vector<float> reference(heights.size());
    std::vector<float> swept(heights.size());
    float toLight[3];
    ToLightAt(1.0f, 0.3f, toLight);
    terrain_shadow_reference::ShadowHeights(heights.data(), toLight, reference.data());
    ComputeTerrainShadowHeights(heights.data(), toLight, swept.data());
    float worst = 0.0f;
    for (size_t i = 0; i < heights.size(); ++i)
        worst = std::max(worst, std::fabs(reference[i] - swept[i]));
    Check(worst <= kSweepTolerance, "the terrain shadow sweep matches the per-vertex sweep behind a wall");
}

void CheckShadowScheduleCoalesces()
{
    TerrainShadowSchedule schedule;
    Check(schedule.Due(true, true, false, false, 0.0), "the first terrain shadow map is computed at once");
    schedule.Computed();
    double seconds = 0.0;
    int recomputes = 0;
    auto step = [&](bool structural, bool terrainChanged, bool lightMoved) {
        seconds += kFrameSeconds;
        const bool due = schedule.Due(structural, terrainChanged, lightMoved, true, seconds);
        if (due)
        {
            ++recomputes;
            schedule.Computed();
        }
        return due;
    };
    for (int frame = 0; frame < kTerrainScanPhases; ++frame)
        step(false, true, false);
    while (seconds < 2.0 * kTerrainSettleSeconds + kTerrainScanPhases * kFrameSeconds)
        step(false, false, false);
    Check(recomputes == 1, "a burst of terrain changes is computed once, after it settles");
    recomputes = 0;
    for (int frame = 0; frame < kStreamingFrames; ++frame)
        step(false, true, false);
    const int expected = static_cast<int>(kStreamingFrames * kFrameSeconds / kTerrainMaxDelaySeconds);
    std::printf("     terrain shadow map under constant streaming: %d recomputes in %d frames\n", recomputes,
                kStreamingFrames);
    Check(recomputes >= expected - 1 && recomputes <= expected,
          "constant terrain changes still recompute about once per maximum delay");
    seconds += 2.0 * kTerrainSettleSeconds;
    recomputes = 0;
    Check(step(false, false, true) && recomputes == 1,
          "a light that turned is applied at once when no terrain changed");
    step(false, true, false);
    Check(step(true, true, false), "a moved or reset window is computed at once during a burst");
    schedule.Computed();
    Check(schedule.Due(false, false, false, false, seconds + kFrameSeconds),
          "a shadow map that failed to upload is recomputed at once");
}

void CheckShadowMapUpdates(IDirect3DDevice9* dev)
{
    SyntheticTerrain terrain(MapVertexHeight);
    TerrainShadowMap map;
    float camera[3];
    CameraOverChunk(kCameraChunk, kCameraChunk, camera);
    float toLight[3];
    ToLightAt(1.0f, 0.0f, toLight);
    double seconds = 0.0;
    const bool first = map.Update(dev, camera, kMap, toLight, terrain, seconds);
    Check(first && map.Texture() && map.Recomputes() == 1, "the terrain shadow map is computed on the first frame");
    terrain.Regenerate([](int row, int col) { return MapVertexHeight(row, col) + 1.0f; });
    for (int frame = 0; frame <= kTerrainScanPhases; ++frame)
    {
        seconds += kFrameSeconds;
        map.Update(dev, camera, kMap, toLight, terrain, seconds);
    }
    Check(map.Recomputes() == 1, "the previous terrain shadow map is kept while a tile's chunks come in");
    while (seconds < kTerrainSettleSeconds + (kTerrainScanPhases + 2) * kFrameSeconds)
    {
        seconds += kFrameSeconds;
        map.Update(dev, camera, kMap, toLight, terrain, seconds);
    }
    Check(map.Recomputes() == 2, "the tile is computed once after its chunks have settled");
    float turned[3];
    ToLightAt(1.0f, 0.05f, turned);
    seconds += kFrameSeconds;
    map.Update(dev, camera, kMap, turned, terrain, seconds);
    Check(map.Recomputes() == 3, "a light that turned is applied on the frame it moved");
    CameraOverChunk(kCameraChunk + kTerrainWindowRecentreChunks + 1, kCameraChunk, camera);
    seconds += kFrameSeconds;
    map.Update(dev, camera, kMap, turned, terrain, seconds);
    Check(map.Recomputes() == 4 &&
              map.Window().FirstVertexRow() ==
                  (kCameraChunk + kTerrainWindowRecentreChunks + 1 - kTerrainWindowChunks / 2) * kTerrainChunkCells,
          "a recentred window is computed on the frame it moved");
}

void CheckTerrainShadows()
{
    CheckWindowPlacesClientChunks();
    CheckWindowFollowsLoadingAndMaps();
    CheckSteadyScanIsStaggered();
    CheckRecentreKeepsOverlap();
    CheckShadowHeights();
    CheckSweepMatchesReference();
    CheckSweepOnAWallStillShadows();
    CheckShadowScheduleCoalesces();
}
}

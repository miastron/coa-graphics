#pragma once

namespace performance_scene
{
constexpr int kWarmupFrames = 32;
constexpr int kMeasuredFrames = 60;
constexpr ULONGLONG kQueryTimeoutMs = 10000;
constexpr ULONGLONG kGpuClockWarmupMs = 1500;
constexpr int kHeaviestQuality = 3;
constexpr Vec3 kEyeAboveStreet = {0, -20, 8};
constexpr Vec3 kTargetAboveStreet = {200, 0, 8};
constexpr int kFloodLights = 8;
constexpr float kFloodLightColor[3] = {1.0f, 0.5f, 0.2f};
constexpr float kFloodLightQuadraticAttenuation = 0.015f;
constexpr float kLampHeight = 5.0f;
constexpr float kLampColor[3] = {1.0f, 0.72f, 0.4f};
constexpr float kLampPeakColor = std::max({kLampColor[0], kLampColor[1], kLampColor[2]});
constexpr int kErrorLogLevel = 0;
constexpr float kFullStorm = 1.0f;
constexpr int kShippedLogLevel = 1;
constexpr bool kShafts = true;
constexpr bool kNoShafts = false;
constexpr float kAheadOfTheCamera = 1.0f;
constexpr float kBehindTheCamera = -1.0f;
constexpr bool kSurfacesLit = true;
constexpr bool kSurfacesUnlit = false;
constexpr int kPassMeasurementAttempts = 2;
constexpr int kNoMaps = 0;
constexpr int kCascades = kWorldShadowMapCount;
constexpr float kThicketNearestRow = 4.0f;
constexpr float kThicketFarthestRow = 160.0f;
constexpr float kThicketHalfWidth = 70.0f;
constexpr float kBladeRowSpacing = 1.5f;
constexpr float kBladeSpacing = 1.2f;
constexpr float kBladeWidth = 0.15f;
constexpr float kBladeHeight = 1.0f;
constexpr DWORD kBladeColor = 0xFF3F7A2E;
constexpr int kUpsamplePathView = 4;
constexpr int kImageQuality = 2;
constexpr unsigned char kPathChannelLit = 128;
constexpr D3DVIEWPORT9 kStreetViewport = {0, 0, 1920, 1080, 0, kClientWorldMaxZ};

enum class FogSource
{
    Derived,
    Classic,
    ClassicStorm,
};

enum class PointLights
{
    None,
    Flood,
    Lamps,
    LampsBehind,
};

struct BenchmarkCase
{
    const char* name;
    FogSource fog;
    PointLights lights;
    int logLevel;
    bool lightShafts;
    float temporal;
};

constexpr BenchmarkCase kCases[] = {
    {"derived-none", FogSource::Derived, PointLights::None, kErrorLogLevel, kShafts, 0.85f},
    {"derived-flood8", FogSource::Derived, PointLights::Flood, kErrorLogLevel, kShafts, 0.85f},
    {"derived-lamps8", FogSource::Derived, PointLights::Lamps, kErrorLogLevel, kShafts, 0.85f},
    {"classic-none", FogSource::Classic, PointLights::None, kErrorLogLevel, kShafts, 0.85f},
    {"classic-lamps8", FogSource::Classic, PointLights::Lamps, kErrorLogLevel, kShafts, 0.85f},
    {"classic-storm", FogSource::ClassicStorm, PointLights::None, kErrorLogLevel, kShafts, 0.85f},
    {"classic-storm-lamps8", FogSource::ClassicStorm, PointLights::Lamps, kErrorLogLevel, kShafts, 0.85f},
    {"derived-none-log1", FogSource::Derived, PointLights::None, kShippedLogLevel, kShafts, 0.85f},
    {"derived-none-shafts0", FogSource::Derived, PointLights::None, kErrorLogLevel, kNoShafts, 0.85f},
    {"derived-lamps8-shafts0", FogSource::Derived, PointLights::Lamps, kErrorLogLevel, kNoShafts, 0.85f},
    {"classic-none-shafts0", FogSource::Classic, PointLights::None, kErrorLogLevel, kNoShafts, 0.85f},
    {"derived-none-temporal0", FogSource::Derived, PointLights::None, kErrorLogLevel, kShafts, 0.0f},
    {"derived-flood8-temporal0", FogSource::Derived, PointLights::Flood, kErrorLogLevel, kShafts, 0.0f},
};

enum class Ground
{
    Street,
    Thicket,
};

struct PassCase
{
    const char* name;
    FogSource fog;
    PointLights lights;
    bool surfaceLights;
    int clientShadowMaps;
    Ground ground;
};

constexpr PassCase kPassCases[] = {
    {"derived-none", FogSource::Derived, PointLights::None, kSurfacesLit, kNoMaps, Ground::Street},
    {"derived-flood8", FogSource::Derived, PointLights::Flood, kSurfacesLit, kNoMaps, Ground::Street},
    {"derived-flood8-surfaces0", FogSource::Derived, PointLights::Flood, kSurfacesUnlit, kNoMaps, Ground::Street},
    {"derived-flood8-maps4", FogSource::Derived, PointLights::Flood, kSurfacesLit, kCascades, Ground::Street},
    {"derived-lamps8", FogSource::Derived, PointLights::Lamps, kSurfacesLit, kNoMaps, Ground::Street},
    {"derived-lamps8-behind", FogSource::Derived, PointLights::LampsBehind, kSurfacesLit, kNoMaps, Ground::Street},
    {"classic-none", FogSource::Classic, PointLights::None, kSurfacesLit, kNoMaps, Ground::Street},
    {"classic-none-maps4", FogSource::Classic, PointLights::None, kSurfacesLit, kCascades, Ground::Street},
    {"classic-lamps8", FogSource::Classic, PointLights::Lamps, kSurfacesLit, kNoMaps, Ground::Street},
    {"classic-lamps8-maps4", FogSource::Classic, PointLights::Lamps, kSurfacesLit, kCascades, Ground::Street},
    {"classic-lamps8-behind", FogSource::Classic, PointLights::LampsBehind, kSurfacesLit, kNoMaps, Ground::Street},
    {"classic-lamps8-behind-maps4", FogSource::Classic, PointLights::LampsBehind, kSurfacesLit,
     kCascades, Ground::Street},
    {"derived-none-thicket", FogSource::Derived, PointLights::None, kSurfacesLit, kNoMaps,
     Ground::Thicket},
    {"derived-flood8-thicket", FogSource::Derived, PointLights::Flood, kSurfacesLit, kNoMaps,
     Ground::Thicket},
    {"classic-none-thicket", FogSource::Classic, PointLights::None, kSurfacesLit, kNoMaps,
     Ground::Thicket},
    {"classic-lamps8-thicket", FogSource::Classic, PointLights::Lamps, kSurfacesLit, kNoMaps,
     Ground::Thicket},
    {"classic-lamps8-behind-thicket", FogSource::Classic, PointLights::LampsBehind, kSurfacesLit,
     kNoMaps, Ground::Thicket},
};

struct StreetLamp
{
    float ahead;
    float side;
    float reach;
};

constexpr StreetLamp kStreetLamps[] = {
    {30.0f, -9.0f, 12.0f}, {45.0f, 9.0f, 16.0f}, {60.0f, -9.0f, 10.0f}, {75.0f, 9.0f, 20.0f},
    {90.0f, -9.0f, 14.0f}, {105.0f, 9.0f, 18.0f}, {120.0f, -9.0f, 11.0f}, {140.0f, 9.0f, 15.0f},
};

struct Street
{
    Vec3 ground;
    Vec3 eye;
    Vec3 at;
};

class Timer
{
public:
    ~Timer()
    {
        for (auto* query : {m_event, m_start, m_end, m_frequency, m_disjoint})
            if (query)
                query->Release();
    }

    bool Create(IDirect3DDevice9* device)
    {
        QueryPerformanceFrequency(&m_cpuFrequency);
        if (FAILED(device->CreateQuery(D3DQUERYTYPE_EVENT, &m_event)))
            return false;
        m_gpu = SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &m_start)) &&
                SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &m_end)) &&
                SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &m_frequency)) &&
                SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &m_disjoint));
        return true;
    }

    bool Begin()
    {
        if (m_gpu)
            return SUCCEEDED(m_disjoint->Issue(D3DISSUE_BEGIN)) && SUCCEEDED(m_start->Issue(D3DISSUE_END));
        if (FAILED(m_event->Issue(D3DISSUE_END)) || !Wait())
            return false;
        QueryPerformanceCounter(&m_cpuStart);
        return true;
    }

    bool End()
    {
        if (m_gpu && (FAILED(m_end->Issue(D3DISSUE_END)) || FAILED(m_frequency->Issue(D3DISSUE_END)) ||
                      FAILED(m_disjoint->Issue(D3DISSUE_END))))
            return false;
        return SUCCEEDED(m_event->Issue(D3DISSUE_END));
    }

    bool Resolve(double& milliseconds)
    {
        if (!Wait())
            return false;
        if (!m_gpu)
        {
            LARGE_INTEGER end;
            QueryPerformanceCounter(&end);
            milliseconds = 1000.0 * (end.QuadPart - m_cpuStart.QuadPart) / m_cpuFrequency.QuadPart;
            return true;
        }
        UINT64 start = 0;
        UINT64 end = 0;
        UINT64 frequency = 0;
        BOOL disjoint = TRUE;
        if (!Wait(m_start, &start, sizeof(start)) || !Wait(m_end, &end, sizeof(end)) ||
            !Wait(m_frequency, &frequency, sizeof(frequency)) || !Wait(m_disjoint, &disjoint, sizeof(disjoint)) ||
            disjoint || !frequency || end < start)
            return false;
        milliseconds = 1000.0 * static_cast<double>(end - start) / static_cast<double>(frequency);
        return true;
    }

    const char* Method() const { return m_gpu ? "GPU timestamps" : "event-flushed CPU/GPU elapsed time"; }

private:
    bool Wait() { return Wait(m_event, nullptr, 0); }

    static bool Wait(IDirect3DQuery9* query, void* data, DWORD size)
    {
        const ULONGLONG started = GetTickCount64();
        for (;;)
        {
            const HRESULT result = query->GetData(data, size, D3DGETDATA_FLUSH);
            if (result != S_FALSE)
                return result == S_OK;
            if (GetTickCount64() - started >= kQueryTimeoutMs)
                return false;
            Sleep(0);
        }
    }

    IDirect3DQuery9* m_event = nullptr;
    IDirect3DQuery9* m_start = nullptr;
    IDirect3DQuery9* m_end = nullptr;
    IDirect3DQuery9* m_frequency = nullptr;
    IDirect3DQuery9* m_disjoint = nullptr;
    LARGE_INTEGER m_cpuFrequency = {};
    LARGE_INTEGER m_cpuStart = {};
    bool m_gpu = false;
};

Vec3 Scaled(Vec3 v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}

Street StreetOn(Vec3 ground)
{
    return {ground, Add(kEyeAboveStreet, ground), Add(kTargetAboveStreet, ground)};
}

void AddThicket(std::vector<SceneVertex>& scene)
{
    int row = 0;
    for (float x = kThicketNearestRow; x < kThicketFarthestRow; x += kBladeRowSpacing, ++row)
    {
        const float stagger = (row % 2) * kBladeSpacing * 0.5f;
        for (float y = kEyeAboveStreet.y - kThicketHalfWidth + stagger; y < kEyeAboveStreet.y + kThicketHalfWidth;
             y += kBladeSpacing)
            AddQuad(scene, {x, y, 0}, {x, y + kBladeWidth, 0}, {x, y + kBladeWidth, kBladeHeight},
                    {x, y, kBladeHeight}, kBladeColor);
    }
}

std::vector<SceneVertex> SceneOn(Ground ground)
{
    std::vector<SceneVertex> scene = BuildScene();
    if (ground == Ground::Thicket)
        AddThicket(scene);
    return scene;
}

std::vector<SceneVertex> SceneAlong(const Street& street, Ground ground = Ground::Street)
{
    std::vector<SceneVertex> scene = SceneOn(ground);
    const Vec3 shift = Sub(street.ground, kGameLikeWorldOffset);
    for (SceneVertex& vertex : scene)
    {
        vertex.x += shift.x;
        vertex.y += shift.y;
        vertex.z += shift.z;
    }
    return scene;
}

void UseHarbourSunset(FrameInputs& inputs)
{
    const FrameInputs harbour =
        ContinentFrame(kEasternKingdoms, kHarbourEye, kHarbourDayFraction, kHarbourToLight, false);
    inputs.mapId = harbour.mapId;
    inputs.dayFraction = harbour.dayFraction;
    std::memcpy(inputs.toLight, harbour.toLight, sizeof(inputs.toLight));
    inputs.lightIsMoon = harbour.lightIsMoon;
    inputs.fogStart = harbour.fogStart;
    inputs.fogEnd = harbour.fogEnd;
    inputs.zoneFogDistance = harbour.zoneFogDistance;
}

FrameInputs StreetInputs(const Street& street, FogSource fog, const float* projection, const D3DVIEWPORT9& viewport)
{
    float view[16];
    CameraRelativeLookAt(street.eye, street.at, view);
    FrameInputs inputs = MakeInputs(view, projection, street.eye, street.at, viewport);
    if (fog != FogSource::Derived)
        UseHarbourSunset(inputs);
    if (fog == FogSource::ClassicStorm)
        inputs.lightParams.stormBlend = kFullStorm;
    return inputs;
}

bool AddPointLight(FrameInputs& inputs, Vec3 position, const float* color, float quadraticAttenuation)
{
    LocalPointLight light = {};
    light.position[0] = position.x;
    light.position[1] = position.y;
    light.position[2] = position.z;
    std::memcpy(light.color, color, sizeof(light.color));
    light.attenuation[0] = 1.0f;
    light.attenuation[2] = quadraticAttenuation;
    const PointLightUpload upload = LocalLightUpload(Config{}, inputs.mapId);
    return engine::SelectLocalPointLight(inputs.localLights, light, inputs.camPos, upload);
}

bool AddFloodLights(FrameInputs& inputs, const Street& street)
{
    for (int index = 0; index < kFloodLights; ++index)
    {
        const Vec3 position = {street.ground.x + 25.0f + 65.0f * (index / 4),
                               street.ground.y - 45.0f + 30.0f * (index % 4), street.ground.z + 12.0f};
        if (!AddPointLight(inputs, position, kFloodLightColor, kFloodLightQuadraticAttenuation))
            return false;
    }
    return true;
}

float QuadraticAttenuationReaching(float reach)
{
    return (kLampPeakColor / kLocalPointLightContributionCutoff - 1.0f) / (reach * reach);
}

bool AddStreetLamps(FrameInputs& inputs, const Street& street, float alongTheView)
{
    const Vec3 forward = Norm(Sub(street.at, street.eye));
    const Vec3 right = Norm(Cross(forward, kWorldUp));
    for (const StreetLamp& lamp : kStreetLamps)
    {
        const Vec3 alongStreet =
            Add(street.eye, Add(Scaled(forward, alongTheView * lamp.ahead), Scaled(right, lamp.side)));
        const Vec3 position = {alongStreet.x, alongStreet.y, street.ground.z + kLampHeight};
        if (!AddPointLight(inputs, position, kLampColor, QuadraticAttenuationReaching(lamp.reach)))
            return false;
    }
    return true;
}

bool AddPointLights(FrameInputs& inputs, PointLights lights, const Street& street)
{
    if (lights == PointLights::Flood)
        return AddFloodLights(inputs, street);
    if (lights == PointLights::Lamps)
        return AddStreetLamps(inputs, street, kAheadOfTheCamera);
    if (lights == PointLights::LampsBehind)
        return AddStreetLamps(inputs, street, kBehindTheCamera);
    return true;
}

void DescribeLights(const char* name, const FrameInputs& inputs)
{
    const LocalLightInputs& lights = inputs.localLights;
    float nearest = std::numeric_limits<float>::infinity();
    float farthest = 0.0f;
    float shortestReach = kMaxLocalPointLightRadius;
    float longestReach = 0.0f;
    for (uint32_t i = 0; i < lights.pointLightCount; ++i)
    {
        const LocalPointLight& light = lights.pointLights[i];
        const Vec3 offset = Sub({light.position[0], light.position[1], light.position[2]},
                                {inputs.camPos[0], inputs.camPos[1], inputs.camPos[2]});
        const float distance = std::sqrt(Dot(offset, offset));
        nearest = std::fmin(nearest, distance);
        farthest = std::fmax(farthest, distance);
        shortestReach = std::fmin(shortestReach, light.cutoff);
        longestReach = std::fmax(longestReach, light.cutoff);
    }
    std::printf("%s: %u point lights reaching %.1f-%.1f yd, %.0f-%.0f yd from the camera\n", name,
                lights.pointLightCount, shortestReach, longestReach, nearest, farthest);
}

std::string FogDataBesideTheFogDll()
{
    char path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(GetModuleHandleA("CoAGraphicsTest.dll"), path, MAX_PATH);
    const std::string module(path, length);
    return module.substr(0, module.find_last_of("\\/") + 1) + "fogdata.bin";
}

bool ClassicFogResolves(const FrameInputs& inputs, int& layers, int& noisyLayers)
{
    FogData data;
    AuthoredFog fog = {};
    const bool resolved = data.Load(FogDataBesideTheFogDll()) &&
                          data.Resolve(inputs.mapId, inputs.camPos, inputs.dayFraction, inputs.lightParams, fog);
    layers = fog.layerCount;
    noisyLayers = 0;
    for (int i = 0; i < fog.layerCount; ++i)
        noisyLayers += fog.layers[i].noise.presence > 0.0f ? 1 : 0;
    return resolved;
}

bool Measure(Harness& h, Timer& timer, const Config& config, const FrameInputs& inputs, Vec3 eye,
             std::vector<double>& samples)
{
    vf_test_set_config(&config);
    for (int frame = 0; frame < kWarmupFrames + kMeasuredFrames; ++frame)
    {
        h.BeginFrame();
        h.DrawScene(eye, inputs.cameraRelativeView, inputs.glProjection, inputs.viewport);
        const bool started = timer.Begin();
        const char* reason = "";
        const bool rendered = started && vf_test_render(&inputs, &reason);
        const bool ended = timer.End();
        h.dev->EndScene();
        double milliseconds = 0;
        if (!rendered || !ended || !timer.Resolve(milliseconds))
        {
            std::printf("performance measurement failed: %s\n", reason);
            return false;
        }
        if (frame >= kWarmupFrames)
            samples.push_back(milliseconds);
        if (FAILED(h.dev->Present(nullptr, nullptr, nullptr, nullptr)))
            return false;
    }
    return true;
}

void PrintTiming(int quality, const char* name, uint32_t pointLights, std::vector<double>& samples)
{
    std::sort(samples.begin(), samples.end());
    const double median = (samples[kMeasuredFrames / 2 - 1] + samples[kMeasuredFrames / 2]) * 0.5;
    const double p95 = samples[(kMeasuredFrames * 95 + 99) / 100 - 1];
    std::printf("%d,%s,%u,%.3f,%.3f\n", quality, name, pointLights, median, p95);
    std::fflush(stdout);
}

bool RenderUntimed(Harness& h, const FrameInputs& inputs, Vec3 eye, int frames)
{
    for (int frame = 0; frame < frames; ++frame)
    {
        h.BeginFrame();
        h.DrawScene(eye, inputs.cameraRelativeView, inputs.glProjection, inputs.viewport);
        const char* reason = "";
        const bool rendered = vf_test_render(&inputs, &reason) != 0;
        h.dev->EndScene();
        if (!rendered)
        {
            std::printf("pass measurement failed: %s\n", reason);
            return false;
        }
        if (FAILED(h.dev->Present(nullptr, nullptr, nullptr, nullptr)))
            return false;
    }
    return true;
}

bool MeasurePasses(Harness& h, const Config& config, const FrameInputs& inputs, Vec3 eye, FogPassTimes& times)
{
    vf_test_set_config(&config);
    for (int attempt = 0; attempt < kPassMeasurementAttempts; ++attempt)
    {
        if (!RenderUntimed(h, inputs, eye, kWarmupFrames))
            return false;
        vf_test_take_fog_pass_times(&times);
        if (!RenderUntimed(h, inputs, eye, kMeasuredFrames))
            return false;
        vf_test_take_fog_pass_times(&times);
        if (times.fog.frames >= kMeasuredFrames / 2)
            return true;
    }
    std::printf("pass measurement failed: the fog timer gave %u samples over %d frames\n", times.fog.frames,
                kMeasuredFrames);
    return false;
}

void PrintPassTimes(int quality, const char* name, uint32_t pointLights, const FogPassTimes& times)
{
    std::printf("%d,%s,%u", quality, name, pointLights);
    for (const GpuTime* time : {&times.fog, &times.march, &times.temporal, &times.surfaceLights, &times.composite})
    {
        if (time->frames > 0)
            std::printf(",%.3f", time->medianMs);
        else
            std::printf(",-");
    }
    std::printf(",%u\n", times.fog.frames);
    std::fflush(stdout);
}

Config ConfigFor(const PassCase& passCase, int quality)
{
    Config config;
    config.quality = quality;
    config.logLevel = kShippedLogLevel;
    config.overlay = false;
    config.dataMode = passCase.fog != FogSource::Derived ? 1 : 0;
    if (!passCase.surfaceLights)
    {
        config.surfaceLights = 0.0f;
        config.spellSurfaceLights = 0.0f;
    }
    return config;
}

void StreetProjection(float* projection)
{
    EngineGlDepthProjection(1.0f / std::tan(kFovY * 0.5f), 1920.0f / 1080.0f, kNear, kFar, projection);
}

struct UpsamplePaths
{
    size_t matchingTaps = 0;
    size_t betweenTaps = 0;
    size_t marchedAgain = 0;
    size_t pixels = 0;
};

UpsamplePaths CountUpsamplePaths(const Image& view)
{
    UpsamplePaths paths;
    for (UINT y = 0; y < view.h; ++y)
    {
        for (UINT x = 0; x < view.w; ++x)
        {
            const unsigned char* bgr = view.At(x, y);
            paths.matchingTaps += bgr[0] >= kPathChannelLit ? 1 : 0;
            paths.betweenTaps += bgr[1] >= kPathChannelLit ? 1 : 0;
            paths.marchedAgain += bgr[2] >= kPathChannelLit ? 1 : 0;
        }
    }
    paths.pixels = static_cast<size_t>(view.w) * view.h;
    return paths;
}

bool RenderAndCapture(Harness& h, const Config& config, const FrameInputs& inputs, Vec3 eye, int settleFrames,
                      Image& image)
{
    vf_test_set_config(&config);
    if (!RenderUntimed(h, inputs, eye, settleFrames))
        return false;
    h.BeginFrame();
    h.DrawScene(eye, inputs.cameraRelativeView, inputs.glProjection, inputs.viewport);
    const char* reason = "";
    const bool rendered = vf_test_render(&inputs, &reason) != 0;
    if (rendered)
        image = Capture(h.dev);
    h.dev->EndScene();
    if (!rendered)
        std::printf("capture failed: %s\n", reason);
    return rendered && SUCCEEDED(h.dev->Present(nullptr, nullptr, nullptr, nullptr));
}

bool ShowUpsamplePathsWithImages(Harness& h, const std::wstring& outDir)
{
    CreateDirectoryW(outDir.c_str(), nullptr);
    float projection[16];
    StreetProjection(projection);
    const Street derivedStreet = StreetOn(kGameLikeWorldOffset);
    const Street harbourStreet = StreetOn(Sub(kHarbourEye, kEyeAboveStreet));
    int layers = 0;
    int noisyLayers = 0;
    const bool classic = ClassicFogResolves(
        StreetInputs(harbourStreet, FogSource::Classic, projection, kStreetViewport), layers, noisyLayers);
    std::printf("where the composite takes each pixel's fog from at 1920x1080 (DebugView=%d), percent of the "
                "viewport; images of the quality %d thicket cases in %ls\n",
                kUpsamplePathView, kImageQuality, outDir.c_str());
    std::printf("quality,case,point_lights,matching_taps,between_taps,marched_again\n");
    for (int quality = 1; quality <= 3; ++quality)
    {
        for (const PassCase& passCase : kPassCases)
        {
            const bool classicCase = passCase.fog != FogSource::Derived;
            if ((classicCase && !classic) || passCase.clientShadowMaps != kNoMaps || !passCase.surfaceLights)
                continue;
            const Street& street = classicCase ? harbourStreet : derivedStreet;
            FrameInputs inputs = StreetInputs(street, passCase.fog, projection, kStreetViewport);
            if (!AddPointLights(inputs, passCase.lights, street))
                return false;
            h.scene = SceneAlong(street, passCase.ground);
            Config config = ConfigFor(passCase, quality);
            Image composited;
            Image pathView;
            if (!RenderAndCapture(h, config, inputs, street.eye, kWarmupFrames, composited))
                return false;
            config.debugView = kUpsamplePathView;
            if (!RenderAndCapture(h, config, inputs, street.eye, 1, pathView))
                return false;
            const UpsamplePaths paths = CountUpsamplePaths(pathView);
            const double percent = 100.0 / static_cast<double>(paths.pixels);
            std::printf("%d,%s,%u,%.2f,%.2f,%.2f\n", quality, passCase.name, inputs.localLights.pointLightCount,
                        paths.matchingTaps * percent, paths.betweenTaps * percent, paths.marchedAgain * percent);
            std::fflush(stdout);
            if (quality == kImageQuality && passCase.ground == Ground::Thicket)
            {
                const std::string narrow = passCase.name;
                const std::wstring stem = outDir + L"\\" + std::wstring(narrow.begin(), narrow.end());
                if (!SavePng(stem + L".png", composited.w, composited.h, composited.bgra) ||
                    !SavePng(stem + L"-paths.png", pathView.w, pathView.h, pathView.bgra))
                {
                    std::printf("could not save %ls.png\n", stem.c_str());
                    return false;
                }
            }
        }
    }
    return true;
}

bool ShowUpsamplePaths(Harness& h, const std::wstring& outDir)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shown = ShowUpsamplePathsWithImages(h, outDir);
    CoUninitialize();
    return shown;
}

bool MeasurePassCases(Harness& h, const Street& derivedStreet, const Street& harbourStreet, bool classic,
                      const float* projection, const D3DVIEWPORT9& viewport, IDirect3DTexture9* clientShadowMap)
{
    std::printf("fog passes at LogLevel %d, the timers the game logs; surface lights include their scene copy; "
                "maps4 cases have four open client shadow cascades of %u px; thicket cases stand blades %.2f yd wide "
                "and %.0f yd tall every %.1f yd across the street\n",
                kShippedLogLevel, surface_light_checks::kShadowMapSize, kBladeWidth, kBladeHeight, kBladeSpacing);
    std::printf("quality,case,point_lights,fog_ms,march_ms,temporal_ms,surface_lights_ms,composite_ms,samples\n");
    for (int quality = 1; quality <= 3; ++quality)
    {
        for (const PassCase& passCase : kPassCases)
        {
            const bool classicCase = passCase.fog != FogSource::Derived;
            if (classicCase && !classic)
                continue;
            const Street& street = classicCase ? harbourStreet : derivedStreet;
            const Config config = ConfigFor(passCase, quality);
            FrameInputs inputs = StreetInputs(street, passCase.fog, projection, viewport);
            if (!AddPointLights(inputs, passCase.lights, street))
                return false;
            h.scene = SceneAlong(street, passCase.ground);
            const WorldShadowInputs clientShadows = surface_light_checks::ClientShadows(
                clientShadowMap, passCase.clientShadowMaps, surface_light_checks::kNoShadowingMap);
            vf_test_override_world_shadows(&clientShadows);
            FogPassTimes times;
            const bool measured = MeasurePasses(h, config, inputs, street.eye, times);
            vf_test_override_world_shadows(nullptr);
            if (!measured)
                return false;
            PrintPassTimes(quality, passCase.name, inputs.localLights.pointLightCount, times);
        }
    }
    return true;
}

bool MeasureGrading(Harness& h, Timer& timer, const Street& street, const float* projection,
                    const D3DVIEWPORT9& viewport)
{
    float curve[kGradingCurveEntries];
    forever_look_checks::SyntheticCurve(curve);
    const ForeverLookFrame look = forever_look_checks::GradingFrame(1.0f, curve);
    Config config;
    config.overlay = false;
    config.logLevel = kErrorLogLevel;
    config.colorGrading = 1.0f;
    vf_test_set_config(&config);
    vf_test_use_forever_look_frame(&look);
    const FrameInputs inputs = StreetInputs(street, FogSource::Derived, projection, viewport);
    h.scene = SceneAlong(street);
    std::vector<double> samples;
    for (int frame = 0; frame < kWarmupFrames + kMeasuredFrames; ++frame)
    {
        GradingStats before;
        GradingStats after;
        h.BeginFrame();
        h.DrawScene(street.eye, inputs.cameraRelativeView, inputs.glProjection, inputs.viewport);
        forever_look_checks::CaptureWorldViewport(h.dev, inputs.viewport);
        vf_test_forever_look_world_done();
        vf_test_grading_stats(&before);
        const bool started = timer.Begin();
        vf_test_hook_frame_end();
        const bool ended = timer.End();
        vf_test_grading_stats(&after);
        h.dev->EndScene();
        double milliseconds = 0;
        if (after.grades != before.grades + 1 || !started || !ended || !timer.Resolve(milliseconds))
        {
            std::printf("colour grading measurement failed\n");
            return false;
        }
        if (frame >= kWarmupFrames)
            samples.push_back(milliseconds);
        if (FAILED(h.dev->Present(nullptr, nullptr, nullptr, nullptr)))
            return false;
    }
    Config off = config;
    off.colorGrading = 0.0f;
    vf_test_set_config(&off);
    PrintTiming(0, "colour-grading", 0, samples);
    return true;
}

bool WarmUpGpuClocks(Harness& h, Timer& timer, const Street& street, const float* projection,
                     const D3DVIEWPORT9& viewport)
{
    Config config;
    config.quality = kHeaviestQuality;
    config.logLevel = kErrorLogLevel;
    config.overlay = false;
    config.dataMode = 0;
    FrameInputs inputs = StreetInputs(street, FogSource::Derived, projection, viewport);
    if (!AddFloodLights(inputs, street))
        return false;
    h.scene = SceneAlong(street);
    const ULONGLONG started = GetTickCount64();
    int frames = 0;
    do
    {
        std::vector<double> untimed;
        if (!Measure(h, timer, config, inputs, street.eye, untimed))
            return false;
        frames += kWarmupFrames + kMeasuredFrames;
    } while (GetTickCount64() - started < kGpuClockWarmupMs);
    std::printf("GPU clock warm-up: %d untimed frames of quality %d with flood8\n", frames, kHeaviestQuality);
    return true;
}

bool MeasureCases(Harness& h)
{
    Timer timer;
    if (!timer.Create(h.dev))
        return false;
    D3DADAPTER_IDENTIFIER9 adapter = {};
    D3DCAPS9 caps = {};
    h.d3d->GetAdapterIdentifier(0, 0, &adapter);
    h.dev->GetDeviceCaps(&caps);
    MultisamplingStatus multisampling;
    vf_test_multisampling(&multisampling);
    std::printf("1080p fog benchmark: %s; %s\n", adapter.Description, timer.Method());
    if (multisampling.method && *multisampling.method)
        std::printf("multisampling %dx kept, depth copied by %s\n", multisampling.samples, multisampling.method);
    else
        std::printf("multisampling off: %s\n", multisampling.off);
    std::printf("pixel shader slots %lu, executed instructions %lu; %d warmup and %d measured frames\n",
                caps.MaxPixelShader30InstructionSlots, caps.MaxPShaderInstructionsExecuted,
                kWarmupFrames, kMeasuredFrames);
    float projection[16];
    StreetProjection(projection);
    const D3DVIEWPORT9 viewport = kStreetViewport;
    const Street derivedStreet = StreetOn(kGameLikeWorldOffset);
    const Street harbourStreet = StreetOn(Sub(kHarbourEye, kEyeAboveStreet));

    FrameInputs described = StreetInputs(derivedStreet, FogSource::Derived, projection, viewport);
    if (!AddFloodLights(described, derivedStreet))
        return false;
    DescribeLights("flood8", described);
    described = StreetInputs(derivedStreet, FogSource::Derived, projection, viewport);
    if (!AddStreetLamps(described, derivedStreet, kAheadOfTheCamera))
        return false;
    DescribeLights("lamps8", described);
    described = StreetInputs(derivedStreet, FogSource::Derived, projection, viewport);
    if (!AddStreetLamps(described, derivedStreet, kBehindTheCamera))
        return false;
    DescribeLights("lamps8-behind", described);
    int classicLayers = 0;
    int noisyLayers = 0;
    const bool classic = ClassicFogResolves(StreetInputs(harbourStreet, FogSource::Classic, projection, viewport),
                                            classicLayers, noisyLayers);
    int stormLayers = 0;
    int noisyStormLayers = 0;
    ClassicFogResolves(StreetInputs(harbourStreet, FogSource::ClassicStorm, projection, viewport), stormLayers,
                       noisyStormLayers);
    if (classic)
        std::printf("classic: the street moved to the Stormwind harbour at 18:43, %d Classic fog layers (%d with "
                    "authored noise); in a full storm %d layers (%d with authored noise)\n",
                    classicLayers, noisyLayers, stormLayers, noisyStormLayers);
    else
        std::printf("classic cases skipped: fogdata.bin beside CoAGraphicsTest.dll does not resolve the harbour\n");

    if (!WarmUpGpuClocks(h, timer, derivedStreet, projection, viewport))
        return false;
    std::printf("quality,case,point_lights,median_ms,p95_ms\n");
    for (int quality = 1; quality <= 3; ++quality)
    {
        for (const BenchmarkCase& benchmark : kCases)
        {
            const bool classicCase = benchmark.fog != FogSource::Derived;
            if (classicCase && !classic)
                continue;
            const Street& street = classicCase ? harbourStreet : derivedStreet;
            Config config;
            config.quality = quality;
            config.logLevel = benchmark.logLevel;
            config.overlay = false;
            config.dataMode = classicCase ? 1 : 0;
            config.lightShafts = benchmark.lightShafts;
            config.temporal = benchmark.temporal;
            FrameInputs inputs = StreetInputs(street, benchmark.fog, projection, viewport);
            if (!AddPointLights(inputs, benchmark.lights, street))
                return false;
            h.scene = SceneAlong(street);
            std::vector<double> samples;
            if (!Measure(h, timer, config, inputs, street.eye, samples))
                return false;
            PrintTiming(quality, benchmark.name, inputs.localLights.pointLightCount, samples);
        }
    }
    IDirect3DTexture9* const clientShadowMap = surface_light_checks::CreateClientShadowMap(h.dev);
    const bool passesMeasured =
        clientShadowMap &&
        MeasurePassCases(h, derivedStreet, harbourStreet, classic, projection, viewport, clientShadowMap);
    if (clientShadowMap)
        clientShadowMap->Release();
    return passesMeasured && MeasureGrading(h, timer, derivedStreet, projection, viewport);
}
}

int RunPerformance(D3DMULTISAMPLE_TYPE samples, const std::function<bool(Harness&)>& measure)
{
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"vfog_performance";
    RegisterClassW(&windowClass);
    Harness h;
    h.window = CreateWindowW(windowClass.lpszClassName, L"vfog performance", WS_OVERLAPPEDWINDOW,
                             0, 0, 1920, 1080, nullptr, nullptr, windowClass.hInstance, nullptr);
    HMODULE module = LoadLibraryA("d3d9.dll");
    const auto create = module ?
        reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(module, "Direct3DCreate9")) : nullptr;
    Config initial;
    initial.overlay = false;
    initial.logLevel = performance_scene::kErrorLogLevel;
    vf_test_set_config(&initial);
    if (h.window && create)
        h.d3d = vf_test_wrap_direct3d9(create, D3D_SDK_VERSION);
    h.pp.Windowed = TRUE;
    h.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    h.pp.BackBufferWidth = 1920;
    h.pp.BackBufferHeight = 1080;
    h.pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    h.pp.EnableAutoDepthStencil = TRUE;
    h.pp.AutoDepthStencilFormat = kClientDepthFormat;
    h.pp.MultiSampleType = samples;
    h.pp.hDeviceWindow = h.window;
    h.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    const DWORD flags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
    bool completed = false;
    if (h.d3d && SUCCEEDED(h.d3d->CreateDevice(0, D3DDEVTYPE_HAL, h.window, flags, &h.pp, &h.dev)) && h.dev)
        completed = measure(h);
    if (h.dev)
        h.dev->Release();
    if (h.d3d)
        h.d3d->Release();
    if (module)
        FreeLibrary(module);
    if (h.window)
        DestroyWindow(h.window);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
    return completed ? 0 : 1;
}

#pragma once

namespace water_performance
{
constexpr int kWarmupFrames = 24;
constexpr int kMeasuredFrames = 90;
constexpr int kShippedLogLevel = 1;
constexpr double kClockStepSeconds = 1.0 / 60.0;
constexpr double kStartSeconds = 60.0;
constexpr const char* kGpuMarker = "water gpu ";

struct WaterCase
{
    const char* name;
    void (*apply)(Config&);
};

void Shipped(Config&) {}
void QualityOne(Config& cfg) { cfg.waterQuality = 1; }
void QualityThree(Config& cfg) { cfg.waterQuality = 3; }
void FlatWaves(Config& cfg) { cfg.waterWaves = 0.0f; }
void SmallWaves(Config& cfg) { cfg.waterWaves = 0.1f; }
void WavesBlended(Config& cfg) { cfg.waterWaveRate = 1; }
void NoReflections(Config& cfg) { cfg.waterReflections = 0.0f; }
void NoFoam(Config& cfg)
{
    cfg.waterFoam = 0.0f;
    cfg.waterShoreFoam = 0.0f;
}
void NoSpecular(Config& cfg) { cfg.waterSpecular = 0.0f; }
void FlatNoReflections(Config& cfg)
{
    FlatWaves(cfg);
    NoReflections(cfg);
}
void QualityOneFlatNoReflections(Config& cfg)
{
    QualityOne(cfg);
    FlatNoReflections(cfg);
}

constexpr WaterCase kCases[] = {
    {"shipped", Shipped},
    {"quality1", QualityOne},
    {"quality3", QualityThree},
    {"waves0", FlatWaves},
    {"waves0.1", SmallWaves},
    {"waves-blended", WavesBlended},
    {"reflections0", NoReflections},
    {"foam0", NoFoam},
    {"specular0", NoSpecular},
    {"waves0-reflections0", FlatNoReflections},
    {"quality1-waves0-reflections0", QualityOneFlatNoReflections},
};

bool SummaryDueAfter(int frame)
{
    return frame == kWarmupFrames - 1 || frame == kWarmupFrames + kMeasuredFrames - 1;
}

constexpr int kFramesPerCase = kWarmupFrames + kMeasuredFrames + 1;

double LastWaterGpuMs(const std::string& log)
{
    const size_t at = log.rfind(kGpuMarker);
    return at == std::string::npos ? -1.0 : std::atof(log.c_str() + at + std::strlen(kGpuMarker));
}

bool MeasureCase(water_checks::BasinClient& client, const Config& base, const WaterCase& item)
{
    Config cfg = water_checks::WaterConfig(base);
    cfg.logLevel = kShippedLogLevel;
    item.apply(cfg);
    vf_test_set_config(&cfg);
    const size_t logStart = ReadText(runtime_cost::FogLogBesideTheFogDll()).size();
    bool shaded = true;
    for (int frame = 0; frame < kFramesPerCase; ++frame)
    {
        vf_test_set_water_seconds(kStartSeconds + frame * kClockStepSeconds);
        shaded = client.Render({}).shaded && shaded;
        if (SummaryDueAfter(frame))
            vf_test_force_water_summary();
    }
    const std::string log = runtime_cost::LogWrittenSince(logStart);
    std::printf("%s,%.3f\n", item.name, LastWaterGpuMs(log));
    return shaded;
}

struct RippleCase
{
    const char* name;
    const wchar_t* shot;
    float ripples;
    double startSeconds;
};

constexpr RippleCase kRippleCases[] = {
    {"ripples0-wader-shin", L"wader-shin", 0.0f, ripple_scene::kStandSeconds},
    {"ripples-wader-shin-running", L"wader-shin", 0.5f, ripple_scene::kStandSeconds},
    {"ripples-wader-shin-standing", L"wader-shin", 0.5f, 0.0},
    {"ripples-npc-run-two-units", L"npc-run", 0.5f, 0.0},
};

bool MeasureRippleCase(water_checks::BasinClient& client, const Config& base, const RippleCase& item)
{
    const std::vector<ripple_scene::Shot> shots = ripple_scene::Shots();
    const ripple_scene::Shot* shot = nullptr;
    for (const ripple_scene::Shot& candidate : shots)
        if (std::wcscmp(candidate.name, item.shot) == 0)
            shot = &candidate;
    if (!shot)
        return false;
    Config cfg = ripple_scene::WakeConfig(base);
    cfg.logLevel = kShippedLogLevel;
    cfg.waterRipples = item.ripples;
    vf_test_set_config(&cfg);
    const size_t logStart = ReadText(runtime_cost::FogLogBesideTheFogDll()).size();
    bool shaded = true;
    for (int frame = 0; frame < kFramesPerCase; ++frame)
    {
        const double seconds = item.startSeconds + frame * kClockStepSeconds;
        shaded = ripple_scene::RenderShotFrame(client, *shot, seconds, true).began && shaded;
        if (SummaryDueAfter(frame))
            vf_test_force_water_summary();
    }
    std::printf("%s,%.3f\n", item.name, LastWaterGpuMs(runtime_cost::LogWrittenSince(logStart)));
    return shaded;
}

int RunWaterPerformance(const std::string& waterDataPath)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"vfog_water_performance";
    RegisterClassW(&wc);
    Config startup;
    startup.overlay = false;
    vf_test_set_config(&startup);
    Harness h;
    bool ok = ripple_scene::CreateSceneDevice(h, wc.lpszClassName) &&
              vf_test_load_water_data(waterDataPath.c_str()) != 0;
    if (ok)
    {
        water_checks::BasinClient client(h, water_checks::DefaultWaterView());
        std::printf("water benchmark, %ux%u world viewport; median water GPU ms of the last %d frames\n",
                    client.View().world.Width, client.View().world.Height, kMeasuredFrames);
        std::printf("case,median_ms\n");
        for (const WaterCase& item : kCases)
            ok = MeasureCase(client, startup, item) && ok;
        for (const RippleCase& item : kRippleCases)
            ok = MeasureRippleCase(client, startup, item) && ok;
        vf_test_set_water_seconds(water_checks::kRealTime);
    }
    h.ReleaseEngineObjects();
    if (h.dev)
        h.dev->Release();
    if (h.d3d)
        h.d3d->Release();
    if (h.window)
        DestroyWindow(h.window);
    CoUninitialize();
    return ok ? 0 : 1;
}
}

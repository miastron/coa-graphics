#pragma once

namespace copy_cost_scene
{
constexpr int kWarmupRounds = 8;
constexpr int kMeasuredRounds = 60;
constexpr DWORD kDeviceFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
constexpr D3DFORMAT kBackBufferFormat = D3DFMT_X8R8G8B8;
constexpr D3DFORMAT kStencilDepthFormat = D3DFMT_D24S8;
constexpr Vec3 kEyeAboveStreet = {0, 0, 9};
constexpr Vec3 kTargetAlongStreet = {100, 2, 4};

struct Resolution
{
    const char* name;
    UINT width;
    UINT height;
};

constexpr Resolution kResolutions[] = {{"deck", 1280, 800}, {"1080p", 1920, 1080}};
constexpr D3DMULTISAMPLE_TYPE kSampleCounts[] = {D3DMULTISAMPLE_NONE, D3DMULTISAMPLE_4_SAMPLES,
                                                 D3DMULTISAMPLE_8_SAMPLES};

struct Camera
{
    Vec3 eye = Add(kEyeAboveStreet, kGameLikeWorldOffset);
    D3DVIEWPORT9 viewport = {};
    float view[16] = {};
    float proj[16] = {};

    explicit Camera(const Resolution& r)
    {
        viewport = {0, 0, r.width, r.height, 0.0f, 1.0f};
        EngineProjection(static_cast<float>(r.width) / r.height, proj);
        CameraRelativeLookAt(eye, Add(kTargetAlongStreet, kGameLikeWorldOffset), view);
    }
};

struct Cost
{
    bool measured = false;
    double medianMs = 0.0;
};

double Median(std::vector<double> samples)
{
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

bool OpenDevice(Harness& h, const Resolution& r, D3DMULTISAMPLE_TYPE samples)
{
    h.pp = {};
    h.pp.Windowed = TRUE;
    h.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    h.pp.BackBufferWidth = r.width;
    h.pp.BackBufferHeight = r.height;
    h.pp.BackBufferFormat = kBackBufferFormat;
    h.pp.MultiSampleType = samples;
    h.pp.EnableAutoDepthStencil = TRUE;
    h.pp.AutoDepthStencilFormat = kStencilDepthFormat;
    h.pp.hDeviceWindow = h.window;
    h.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (FAILED(h.d3d->CreateDevice(0, D3DDEVTYPE_HAL, h.window, kDeviceFlags, &h.pp, &h.dev)) || !h.dev)
    {
        h.dev = nullptr;
        return false;
    }
    h.CreateEngineObjects();
    return true;
}

void CloseDevice(Harness& h)
{
    if (!h.dev)
        return;
    h.ReleaseEngineObjects();
    h.dev->Release();
    h.dev = nullptr;
}

template <typename Operation>
Cost TimeAfterSceneDraws(Harness& h, performance_scene::Timer& timer, const Camera& camera, Operation operation)
{
    std::vector<double> samples;
    for (int round = 0; round < kWarmupRounds + kMeasuredRounds; ++round)
    {
        h.BeginFrame();
        h.DrawScene(camera.eye, camera.view, camera.proj, camera.viewport);
        double milliseconds = 0.0;
        const bool timed = timer.Begin() && operation() && timer.End() && timer.Resolve(milliseconds);
        h.dev->EndScene();
        h.dev->Present(nullptr, nullptr, nullptr, nullptr);
        if (!timed)
            return {};
        if (round >= kWarmupRounds)
            samples.push_back(milliseconds);
    }
    return {true, Median(samples)};
}

Cost TimeColourCopy(Harness& h, performance_scene::Timer& timer, const Camera& camera)
{
    IDirect3DTexture9* copy = nullptr;
    IDirect3DSurface9* copySurface = nullptr;
    IDirect3DSurface9* target = nullptr;
    Cost cost;
    if (SUCCEEDED(h.dev->CreateTexture(camera.viewport.Width, camera.viewport.Height, 1, D3DUSAGE_RENDERTARGET,
                                       kBackBufferFormat, D3DPOOL_DEFAULT, &copy, nullptr)) &&
        SUCCEEDED(copy->GetSurfaceLevel(0, &copySurface)) && SUCCEEDED(h.dev->GetRenderTarget(0, &target)))
    {
        const RECT world = {0, 0, static_cast<LONG>(camera.viewport.Width), static_cast<LONG>(camera.viewport.Height)};
        cost = TimeAfterSceneDraws(h, timer, camera, [&] {
            return SUCCEEDED(h.dev->StretchRect(target, &world, copySurface, nullptr, D3DTEXF_POINT));
        });
    }
    for (IUnknown* object : std::initializer_list<IUnknown*>{target, copySurface, copy})
        if (object)
            object->Release();
    return cost;
}

Cost TimeDepthRefresh(Harness& h, performance_scene::Timer& timer, const Camera& camera)
{
    return TimeAfterSceneDraws(h, timer, camera, [] { return vf_test_refresh_scene_depth() != 0; });
}

void PrintCost(const Cost& cost)
{
    if (cost.measured)
        std::printf(",%.3f", cost.medianMs);
    else
        std::printf(",none");
}

void MeasureCase(Harness& h, const Resolution& r, D3DMULTISAMPLE_TYPE samples)
{
    std::printf("%s %ux%u,%d", r.name, r.width, r.height, samples ? static_cast<int>(samples) : 1);
    if (!OpenDevice(h, r, samples))
    {
        std::printf(",device not created\n");
        return;
    }
    MultisamplingStatus status;
    vf_test_multisampling(&status);
    D3DSURFACE_DESC backBuffer = {};
    IDirect3DSurface9* target = nullptr;
    if (SUCCEEDED(h.dev->GetRenderTarget(0, &target)))
    {
        target->GetDesc(&backBuffer);
        target->Release();
    }
    const Camera camera(r);
    performance_scene::Timer timer;
    if (!timer.Create(h.dev))
    {
        std::printf(",no timer\n");
        CloseDevice(h);
        return;
    }
    std::printf(",%d,%s", static_cast<int>(backBuffer.MultiSampleType),
                status.method && *status.method ? status.method : status.off);
    PrintCost(TimeColourCopy(h, timer, camera));
    PrintCost(TimeDepthRefresh(h, timer, camera));
    std::printf(",%s\n", timer.Method());
    CloseDevice(h);
}

int RunCopyCost()
{
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"vfog_copy_cost";
    RegisterClassW(&windowClass);
    Harness h;
    h.window = CreateWindowW(windowClass.lpszClassName, L"vfog copy cost", WS_OVERLAPPEDWINDOW, 0, 0, 1920, 1080,
                             nullptr, nullptr, windowClass.hInstance, nullptr);
    HMODULE module = LoadLibraryA("d3d9.dll");
    const auto create =
        module ? reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(module, "Direct3DCreate9")) : nullptr;
    Config cfg;
    cfg.overlay = false;
    cfg.multisampling = true;
    cfg.logLevel = performance_scene::kErrorLogLevel;
    vf_test_set_config(&cfg);
    if (h.window && create)
        h.d3d = vf_test_wrap_direct3d9(create, D3D_SDK_VERSION);
    char d3d9Path[MAX_PATH] = {};
    if (module)
        GetModuleFileNameA(module, d3d9Path, MAX_PATH);
    std::printf("copy cost with %s; median GPU ms of %d rounds, each after a scene draw\n", d3d9Path,
                kMeasuredRounds);
    std::printf("resolution,samples requested,back buffer samples,depth copy,colour copy ms,depth refresh ms,timer\n");
    if (h.d3d)
        for (const Resolution& r : kResolutions)
            for (D3DMULTISAMPLE_TYPE samples : kSampleCounts)
                MeasureCase(h, r, samples);
    const bool completed = h.d3d != nullptr;
    if (h.d3d)
        h.d3d->Release();
    if (module)
        FreeLibrary(module);
    if (h.window)
        DestroyWindow(h.window);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
    return completed ? 0 : 1;
}
}

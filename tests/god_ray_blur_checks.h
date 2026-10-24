#include "god_ray_blur.h"
#include "ps_ray_blur.h"

namespace god_ray_blur_checks
{
constexpr UINT kSourceSize = 64;
constexpr UINT kTargetSize = 32;
constexpr double kSunU = 0.5;
constexpr double kSunV = 0.5;
constexpr double kSourceBase = 0.2;
constexpr double kSourceSlopeU = 0.5;
constexpr double kSourceSlopeV = 0.3;
constexpr double kShaderTolerance = 2.0e-4;
constexpr int kFirstInteriorTexel = 3;
constexpr int kLastInteriorTexel = static_cast<int>(kTargetSize) - 4;

double SourceAt(double u, double v)
{
    return kSourceBase + kSourceSlopeU * u + kSourceSlopeV * v;
}

double BlurredAt(int pass, double u, double v, bool firstPassFeedsThis)
{
    const god_ray_blur::Pass blurPass = god_ray_blur::PassAt(pass);
    double sum = 0.0;
    double weight = 1.0;
    for (int tap = 0; tap < god_ray_blur::kTaps; ++tap)
    {
        const double fraction = tap * static_cast<double>(blurPass.stepTowardSun);
        const double tapU = u + (kSunU - u) * fraction;
        const double tapV = v + (kSunV - v) * fraction;
        const double previous = firstPassFeedsThis ? BlurredAt(0, tapU, tapV, false) : SourceAt(tapU, tapV);
        sum += previous * weight;
        weight *= blurPass.decayPerTap;
    }
    return sum * blurPass.normalisation;
}

void CheckKernelDecaysWithoutSteps()
{
    constexpr int kTapCount = god_ray_blur::kTaps * god_ray_blur::kTaps;
    double weights[kTapCount] = {};
    double total = 0.0;
    for (int coarse = 0; coarse < god_ray_blur::kTaps; ++coarse)
        for (int fine = 0; fine < god_ray_blur::kTaps; ++fine)
        {
            weights[coarse * god_ray_blur::kTaps + fine] =
                god_ray_blur::TapWeight(0, coarse) * god_ray_blur::TapWeight(1, fine);
            total += weights[coarse * god_ray_blur::kTaps + fine];
        }
    double largestRise = 0.0;
    double largestRatioError = 0.0;
    const double expectedRatio = std::pow(static_cast<double>(god_ray_blur::kDecayPerFirstPassTap),
                                          1.0 / god_ray_blur::kTaps);
    for (int tap = 1; tap < kTapCount; ++tap)
    {
        largestRise = (std::max)(largestRise, weights[tap] / weights[tap - 1] - 1.0);
        largestRatioError = (std::max)(largestRatioError, std::fabs(weights[tap] / weights[tap - 1] - expectedRatio));
    }
    for (int pass = 0; pass < god_ray_blur::kPasses; ++pass)
    {
        double passSum = 0.0;
        for (int tap = 0; tap < god_ray_blur::kTaps; ++tap)
            passSum += god_ray_blur::TapWeight(pass, tap);
        char label[128];
        std::snprintf(label, sizeof(label), "god ray blur pass %d weights sum to one (%.6f)", pass + 1, passSum);
        Check(std::fabs(passSum - 1.0) < 1.0e-5, label);
    }
    char label[160];
    std::snprintf(label, sizeof(label),
                  "god ray blur kernel never rises along a ray (largest rise %.4f, ratio error %.4f)", largestRise,
                  largestRatioError);
    Check(largestRise <= 1.0e-6 && largestRatioError < 0.02 && std::fabs(total - 1.0) < 1.0e-5, label);
}

struct Fixture
{
    IDirect3DDevice9* dev;
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* savedTarget = nullptr;
    IDirect3DSurface9* savedDepth = nullptr;
    IDirect3DVertexBuffer9* savedStream = nullptr;
    UINT savedOffset = 0;
    UINT savedStride = 0;
    IDirect3DTexture9* source = nullptr;
    IDirect3DTexture9* targets[2] = {};
    IDirect3DSurface9* readback = nullptr;
    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    IDirect3DVertexDeclaration9* declaration = nullptr;

    explicit Fixture(IDirect3DDevice9* device) : dev(device) {}

    ~Fixture()
    {
        for (DWORD stage = 0; stage < 2; ++stage)
            dev->SetTexture(stage, nullptr);
        if (savedTarget)
            dev->SetRenderTarget(0, savedTarget);
        dev->SetDepthStencilSurface(savedDepth);
        if (state)
            state->Apply();
        dev->SetStreamSource(0, savedStream, savedOffset, savedStride);
        IUnknown* resources[] = {state, savedTarget, savedDepth, savedStream, source, targets[0], targets[1],
                                 readback, vs, ps, declaration};
        for (IUnknown* resource : resources)
            if (resource)
                resource->Release();
    }

    bool SupportsFilteredFloatTargets() const
    {
        IDirect3D9* d3d = nullptr;
        D3DDEVICE_CREATION_PARAMETERS creation = {};
        D3DDISPLAYMODE mode = {};
        bool supported = false;
        if (SUCCEEDED(dev->GetDirect3D(&d3d)) && d3d)
        {
            supported = SUCCEEDED(dev->GetCreationParameters(&creation)) &&
                        SUCCEEDED(dev->GetDisplayMode(0, &mode)) &&
                        SUCCEEDED(d3d->CheckDeviceFormat(creation.AdapterOrdinal, creation.DeviceType, mode.Format,
                                                         D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_FILTER,
                                                         D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F));
            d3d->Release();
        }
        return supported;
    }

    bool FillSource()
    {
        D3DLOCKED_RECT locked = {};
        if (FAILED(source->LockRect(0, &locked, nullptr, 0)))
            return false;
        for (UINT y = 0; y < kSourceSize; ++y)
        {
            auto* row = reinterpret_cast<float*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
            for (UINT x = 0; x < kSourceSize; ++x)
            {
                const auto value = static_cast<float>(SourceAt((x + 0.5) / kSourceSize, (y + 0.5) / kSourceSize));
                row[4 * x] = row[4 * x + 1] = row[4 * x + 2] = value;
                row[4 * x + 3] = 1.0f;
            }
        }
        return SUCCEEDED(source->UnlockRect(0));
    }

    bool Create()
    {
        if (FAILED(dev->CreateStateBlock(D3DSBT_ALL, &state)))
            return false;
        dev->GetRenderTarget(0, &savedTarget);
        dev->GetDepthStencilSurface(&savedDepth);
        dev->GetStreamSource(0, &savedStream, &savedOffset, &savedStride);
        if (FAILED(dev->CreateTexture(kSourceSize, kSourceSize, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_MANAGED,
                                      &source, nullptr)) ||
            FAILED(dev->CreateTexture(kTargetSize, kTargetSize, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A32B32G32R32F,
                                      D3DPOOL_DEFAULT, &targets[0], nullptr)) ||
            FAILED(dev->CreateTexture(kTargetSize, kTargetSize, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A32B32G32R32F,
                                      D3DPOOL_DEFAULT, &targets[1], nullptr)) ||
            FAILED(dev->CreateOffscreenPlainSurface(kTargetSize, kTargetSize, D3DFMT_A32B32G32R32F,
                                                    D3DPOOL_SYSTEMMEM, &readback, nullptr)) ||
            FAILED(CreateFullscreenVertexShader(dev, &vs)) ||
            FAILED(dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_ray_blur), &ps)) ||
            FAILED(CreateFullscreenVertexDeclaration(dev, &declaration)))
            return false;
        return FillSource();
    }

    void SetDrawState()
    {
        dev->SetDepthStencilSurface(nullptr);
        const D3DVIEWPORT9 viewport = {0, 0, kTargetSize, kTargetSize, 0.0f, 1.0f};
        dev->SetViewport(&viewport);
        dev->SetVertexShader(vs);
        dev->SetPixelShader(ps);
        dev->SetVertexDeclaration(declaration);
        dev->SetStreamSourceFreq(0, 1);
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    }

    bool Blur(int pass, IDirect3DBaseTexture9* input, IDirect3DTexture9* output)
    {
        const god_ray_blur::Pass blurPass = god_ray_blur::PassAt(pass);
        const float constants[3][4] = {
            {static_cast<float>(kSunU), static_cast<float>(kSunV), blurPass.stepTowardSun, blurPass.normalisation},
            {static_cast<float>(kTargetSize), static_cast<float>(kTargetSize), 1.0f / kTargetSize,
             1.0f / kTargetSize},
            {blurPass.decayPerTap, 0.0f, 0.0f, 0.0f},
        };
        IDirect3DSurface9* surface = nullptr;
        if (FAILED(output->GetSurfaceLevel(0, &surface)))
            return false;
        dev->SetRenderTarget(0, surface);
        surface->Release();
        SetDrawState();
        dev->SetPixelShaderConstantF(9, &constants[0][0], 3);
        dev->SetTexture(0, input);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        dev->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
        if (FAILED(dev->BeginScene()))
            return false;
        const HRESULT drawn = DrawFullscreenTriangle(dev);
        return SUCCEEDED(dev->EndScene()) && SUCCEEDED(drawn);
    }

    bool Read(IDirect3DTexture9* texture, float (&red)[kTargetSize][kTargetSize])
    {
        IDirect3DSurface9* surface = nullptr;
        if (FAILED(texture->GetSurfaceLevel(0, &surface)))
            return false;
        const bool copied = SUCCEEDED(dev->GetRenderTargetData(surface, readback));
        surface->Release();
        D3DLOCKED_RECT locked = {};
        if (!copied || FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return false;
        for (UINT y = 0; y < kTargetSize; ++y)
        {
            const BYTE* const line = static_cast<const BYTE*>(locked.pBits) + y * locked.Pitch;
            const auto* row = reinterpret_cast<const float*>(line);
            for (UINT x = 0; x < kTargetSize; ++x)
                red[y][x] = row[4 * x];
        }
        return SUCCEEDED(readback->UnlockRect());
    }
};

double LargestShaderError(const float (&red)[kTargetSize][kTargetSize], int passes)
{
    double largest = 0.0;
    for (int y = kFirstInteriorTexel; y <= kLastInteriorTexel; ++y)
        for (int x = kFirstInteriorTexel; x <= kLastInteriorTexel; ++x)
        {
            const double u = (x + 0.5) / kTargetSize;
            const double v = (y + 0.5) / kTargetSize;
            const double expected = passes == 1 ? BlurredAt(0, u, v, false) : BlurredAt(1, u, v, true);
            largest = (std::max)(largest, std::fabs(red[y][x] - expected));
        }
    return largest;
}

void CheckShaderMatchesKernel(IDirect3DDevice9* dev)
{
    Fixture fixture(dev);
    if (!fixture.SupportsFilteredFloatTargets())
    {
        std::printf("     god ray blur shader check skipped: no filtered float32 targets\n");
        return;
    }
    const bool ready = fixture.Create();
    Check(ready, "god ray blur fixtures created");
    if (!ready)
        return;
    float firstPass[kTargetSize][kTargetSize] = {};
    float secondPass[kTargetSize][kTargetSize] = {};
    const bool drawn = fixture.Blur(0, fixture.source, fixture.targets[0]) &&
                       fixture.Read(fixture.targets[0], firstPass) &&
                       fixture.Blur(1, fixture.targets[0], fixture.targets[1]) &&
                       fixture.Read(fixture.targets[1], secondPass);
    Check(drawn, "god ray blur passes drawn");
    if (!drawn)
        return;
    const double firstError = LargestShaderError(firstPass, 1);
    const double secondError = LargestShaderError(secondPass, 2);
    char label[160];
    std::snprintf(label, sizeof(label), "god ray blur shader follows the kernel (errors %.2e, %.2e)", firstError,
                  secondError);
    Check(firstError < kShaderTolerance && secondError < kShaderTolerance, label);
}

void CheckGodRayBlur(IDirect3DDevice9* dev)
{
    CheckKernelDecaysWithoutSteps();
    CheckShaderMatchesKernel(dev);
}
}

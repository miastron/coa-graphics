#pragma once

#include "shadowed_composite_shaders.h"

namespace glow_upsample
{
using grazing_upsample::kDensity;
using grazing_upsample::kDepthInfinity;
using grazing_upsample::kDepthInverse;
using grazing_upsample::kHeight;
using grazing_upsample::kHorizonBlendStart;
using grazing_upsample::kLightTexels;
using grazing_upsample::kMaxFogDistance;
using grazing_upsample::kWidth;

constexpr UINT kScale = 2;
constexpr UINT kLowWidth = (kWidth + kScale - 1) / kScale;
constexpr UINT kLowHeight = (kHeight + kScale - 1) / kScale;
constexpr float kLightViewZ = 150.0f;
constexpr float kLightRadius = 60.0f;
constexpr float kLightPhase = 0.3f;
constexpr int kDebugRadianceView = 1;
constexpr DWORD kDepthStage = 0;
constexpr DWORD kFilteredFogStage = 1;
constexpr DWORD kCurrentMarchStage = 4;
constexpr DWORD kLightStage = 8;
constexpr float kMaxGlowError = 0.05f;
constexpr float kMinGlowShare = 0.25f;

struct FloatTarget
{
    IDirect3DTexture9* texture = nullptr;
    IDirect3DSurface9* surface = nullptr;
    IDirect3DSurface9* readback = nullptr;
    UINT width = 0;
    UINT height = 0;

    ~FloatTarget()
    {
        IUnknown* resources[] = {texture, surface, readback};
        for (IUnknown* resource : resources)
            if (resource)
                resource->Release();
    }

    bool Create(IDirect3DDevice9* device, UINT w, UINT h)
    {
        width = w;
        height = h;
        return SUCCEEDED(device->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A32B32G32R32F,
                                               D3DPOOL_DEFAULT, &texture, nullptr)) &&
               SUCCEEDED(texture->GetSurfaceLevel(0, &surface)) &&
               SUCCEEDED(device->CreateOffscreenPlainSurface(w, h, D3DFMT_A32B32G32R32F, D3DPOOL_SYSTEMMEM,
                                                             &readback, nullptr));
    }

    bool Read(IDirect3DDevice9* device, std::vector<float>& rgba)
    {
        D3DLOCKED_RECT locked = {};
        if (FAILED(device->GetRenderTargetData(surface, readback)) ||
            FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return false;
        rgba.resize(static_cast<size_t>(width) * height * 4);
        for (UINT y = 0; y < height; ++y)
            std::memcpy(&rgba[static_cast<size_t>(y) * width * 4],
                        static_cast<const BYTE*>(locked.pBits) + y * locked.Pitch, width * 4 * sizeof(float));
        return SUCCEEDED(readback->UnlockRect());
    }
};

struct Fixture
{
    IDirect3DDevice9* device;
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* savedTarget = nullptr;
    IDirect3DSurface9* savedDepth = nullptr;
    IDirect3DTexture9* depth = nullptr;
    IDirect3DTexture9* lights = nullptr;
    IDirect3DPixelShader9* march = nullptr;
    IDirect3DPixelShader9* composite = nullptr;
    FloatTarget lowMarch;
    FloatTarget fullMarch;
    FloatTarget composited;

    explicit Fixture(IDirect3DDevice9* value) : device(value) {}

    ~Fixture()
    {
        for (DWORD stage : {kDepthStage, kFilteredFogStage, kCurrentMarchStage, kLightStage})
            device->SetTexture(stage, nullptr);
        if (savedTarget)
            device->SetRenderTarget(0, savedTarget);
        device->SetDepthStencilSurface(savedDepth);
        if (state)
            state->Apply();
        IUnknown* resources[] = {state, savedTarget, savedDepth, depth, lights, march, composite};
        for (IUnknown* resource : resources)
            if (resource)
                resource->Release();
    }

    bool Create()
    {
        return SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &state)) &&
               SUCCEEDED(device->GetRenderTarget(0, &savedTarget)) &&
               SUCCEEDED(device->GetDepthStencilSurface(&savedDepth)) &&
               SUCCEEDED(device->CreateTexture(kWidth, kHeight, 1, 0, D3DFMT_R32F, D3DPOOL_MANAGED, &depth,
                                               nullptr)) &&
               SUCCEEDED(device->CreateTexture(kLightTexels, 1, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_MANAGED,
                                               &lights, nullptr)) &&
               SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_lit_march_mid), &march)) &&
               SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_shadowed_composite_mid),
                                                   &composite)) &&
               lowMarch.Create(device, kLowWidth, kLowHeight) && fullMarch.Create(device, kWidth, kHeight) &&
               composited.Create(device, kWidth, kHeight);
    }

    bool FillGroundAndLight(float quadraticFalloff)
    {
        D3DLOCKED_RECT locked = {};
        if (FAILED(depth->LockRect(0, &locked, nullptr, 0)))
            return false;
        for (UINT y = 0; y < kHeight; ++y)
        {
            float* row = reinterpret_cast<float*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
            for (UINT x = 0; x < kWidth; ++x)
                row[x] = kDepthInfinity + kDepthInverse * grazing_upsample::InverseGroundViewZ(y + 0.5f);
        }
        if (FAILED(depth->UnlockRect(0)) || FAILED(lights->LockRect(0, &locked, nullptr, 0)))
            return false;
        auto* values = static_cast<float*>(locked.pBits);
        std::memset(values, 0, kLightTexels * 4 * sizeof(float));
        values[2] = kLightViewZ;
        values[3] = kLightRadius;
        values[4] = values[5] = values[6] = 1.0f;
        values[8] = 1.0f;
        values[10] = quadraticFalloff;
        values[11] = 1.0f;
        return SUCCEEDED(lights->UnlockRect(0));
    }
};

void SetPointSampling(IDirect3DDevice9* device)
{
    for (DWORD stage : {kDepthStage, kFilteredFogStage, kCurrentMarchStage, kLightStage})
    {
        device->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
        device->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    }
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
}

bool Draw(Fixture& fixture, FloatTarget& target, IDirect3DPixelShader9* shader, UINT scale)
{
    IDirect3DDevice9* device = fixture.device;
    const D3DVIEWPORT9 viewport = {0, 0, target.width, target.height, 0, 1};
    device->SetDepthStencilSurface(nullptr);
    device->SetRenderTarget(0, target.surface);
    device->SetViewport(&viewport);
    device->SetPixelShader(shader);
    device->SetVertexShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW);
    device->SetTexture(kDepthStage, fixture.depth);
    device->SetTexture(kFilteredFogStage, fixture.lowMarch.texture);
    device->SetTexture(kCurrentMarchStage, fixture.lowMarch.texture);
    device->SetTexture(kLightStage, fixture.lights);
    SetPointSampling(device);
    const UINT lowWidth = (kWidth + scale - 1) / scale;
    const UINT lowHeight = (kHeight + scale - 1) / scale;
    float constants[99][4] = {};
    constants[0][2] = static_cast<float>(kWidth);
    constants[0][3] = static_cast<float>(kHeight);
    constants[1][0] = static_cast<float>(scale);
    constants[1][2] = 1.0f / kWidth;
    constants[1][3] = 1.0f / kHeight;
    constants[2][0] = constants[2][1] = 1;
    constants[3][0] = kDepthInfinity;
    constants[3][1] = kDepthInverse;
    constants[3][2] = kMaxFogDistance;
    constants[3][3] = kDepthInfinity;
    for (int row = 0; row < 4; ++row)
        constants[4 + row][row] = 1;
    constants[8][0] = static_cast<float>(lowWidth);
    constants[8][1] = static_cast<float>(lowHeight);
    constants[8][2] = 1.0f / lowWidth;
    constants[8][3] = 1.0f / lowHeight;
    constants[9][2] = constants[9][3] = 1;
    constants[11][1] = kMaxFogDistance;
    constants[11][2] = kHorizonBlendStart;
    constants[11][3] = kHorizonBlendStart + kMaxFogDistance;
    constants[12][1] = kDensity;
    constants[12][3] = 1;
    constants[14][3] = 1;
    constants[16][3] = 1;
    constants[17][2] = kMaxFogDistance;
    constants[53][0] = 1;
    constants[53][2] = kLightPhase;
    constants[96][0] = 1;
    constants[96][2] = static_cast<float>(kDebugRadianceView);
    device->SetPixelShaderConstantF(0, &constants[0][0], 99);
    const float right = target.width - 0.5f;
    const float bottom = target.height - 0.5f;
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {right, -0.5f, 0, 1}, {-0.5f, bottom, 0, 1},
                              {right, bottom, 0, 1}};
    const HRESULT begin = device->BeginScene();
    const HRESULT draw =
        SUCCEEDED(begin) ? device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0])) : begin;
    if (SUCCEEDED(begin))
        device->EndScene();
    return SUCCEEDED(draw);
}

bool OnTapRow(UINT y)
{
    return y % kScale == kScale / 2;
}

float Luminance(const float* rgba)
{
    return (rgba[0] + rgba[1] + rgba[2]) / 3.0f;
}

float BilinearLowResLuminance(const std::vector<float>& low, UINT x, UINT y)
{
    const float lowX = (x - static_cast<float>(kScale / 2)) / kScale;
    const float lowY = (y - static_cast<float>(kScale / 2)) / kScale;
    const int firstX = static_cast<int>(std::floor(lowX));
    const int firstY = static_cast<int>(std::floor(lowY));
    const float fractionX = lowX - firstX;
    const float fractionY = lowY - firstY;
    auto at = [&low](int tx, int ty) {
        tx = std::clamp(tx, 0, static_cast<int>(kLowWidth) - 1);
        ty = std::clamp(ty, 0, static_cast<int>(kLowHeight) - 1);
        return Luminance(&low[(static_cast<size_t>(ty) * kLowWidth + tx) * 4]);
    };
    const float top = at(firstX, firstY) * (1 - fractionX) + at(firstX + 1, firstY) * fractionX;
    const float under = at(firstX, firstY + 1) * (1 - fractionX) + at(firstX + 1, firstY + 1) * fractionX;
    return top * (1 - fractionY) + under * fractionY;
}

struct FalloffCase
{
    const char* name;
    float quadratic;
};

const FalloffCase kFalloffCases[] = {{"without distance falloff", 0.0f}, {"with a quadratic falloff", 0.01f}};

void CheckGlowWithFalloff(IDirect3DDevice9* device, const FalloffCase& falloff)
{
    Fixture fixture(device);
    const bool ready = fixture.Create() && fixture.FillGroundAndLight(falloff.quadratic);
    Check(ready, "glow upsample fixtures created");
    if (!ready)
        return;
    std::vector<float> low;
    std::vector<float> full;
    std::vector<float> composited;
    const bool drawn = Draw(fixture, fixture.lowMarch, fixture.march, kScale) &&
                       Draw(fixture, fixture.fullMarch, fixture.march, 1) &&
                       Draw(fixture, fixture.composited, fixture.composite, kScale) &&
                       fixture.lowMarch.Read(device, low) && fixture.fullMarch.Read(device, full) &&
                       fixture.composited.Read(device, composited);
    Check(drawn, "glow upsample march and composite drawn and read back");
    if (!drawn)
        return;
    float brightest = 0.0f;
    for (size_t i = 0; i < full.size(); i += 4)
        brightest = std::fmax(brightest, Luminance(&full[i]));
    float correctedError = 0.0f;
    float interpolatedError = 0.0f;
    for (UINT y = 0; y < kHeight; ++y)
        for (UINT x = 0; x < kWidth && !OnTapRow(y); ++x)
        {
            const size_t i = (static_cast<size_t>(y) * kWidth + x) * 4;
            const float reference = Luminance(&full[i]);
            correctedError = std::fmax(correctedError, std::fabs(Luminance(&composited[i]) - reference));
            interpolatedError = std::fmax(interpolatedError, std::fabs(BilinearLowResLuminance(low, x, y) - reference));
        }
    std::printf("     glow on grazing ground %s: brightest %.4f, worst error between the tap rows with the glow "
                "corrected %.4f, interpolated %.4f\n",
                falloff.name, brightest, correctedError, interpolatedError);
    char label[256];
    std::snprintf(label, sizeof(label),
                  "a light %s over grazing ground: between the tap rows the composite interpolates the fog and adds "
                  "each pixel's own glow, within 5%% of the brightest full-resolution march and a quarter of plain "
                  "interpolation's error",
                  falloff.name);
    Check(brightest > 0.0f && correctedError <= kMaxGlowError * brightest &&
              correctedError <= kMinGlowShare * interpolatedError,
          label);
}

void CheckGlowBetweenGrazingTaps(IDirect3DDevice9* device)
{
    for (const FalloffCase& falloff : kFalloffCases)
        CheckGlowWithFalloff(device, falloff);
}
}

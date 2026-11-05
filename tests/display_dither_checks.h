#pragma once

#include "ps_ray_composite.h"

namespace display_dither
{
constexpr UINT kWidth = 256;
constexpr UINT kHeight = 64;
constexpr UINT kPixels = kWidth * kHeight;
constexpr UINT kConstants = 103;
constexpr float kCodeMax = 255.0f;
constexpr int kLastCode = 255;
constexpr int kBlueCodeStride = 37;
constexpr float kSkyDepth = 1.0f;
constexpr float kDepthAtInfiniteViewZ = 0.94f;
constexpr float kDepthPerInverseViewZ = -0.4f;
constexpr float kMaxFogDistance = 1000.0f;
constexpr float kRampFirstCode = 30.0f;
constexpr float kRampLastCode = 38.0f;
constexpr float kClientGlow = 0.65f;
constexpr float kFullRays = 1.0f;
constexpr float kDitherHalfSpan = 0.495f;
constexpr float kUnormConversionTolerance = 0.6f;
constexpr float kMaxPixelError = kUnormConversionTolerance + kDitherHalfSpan;
constexpr float kMaxColumnMeanRms = 0.1f;
constexpr float kMaxColumnMeanError = 0.2f;
constexpr float kMaxMeanBias = 0.1f;
constexpr float kQuarterCode = 0.25f;
constexpr float kRadianceView = 1.0f;
constexpr float kTransmittanceView = 2.0f;
constexpr float kGammaFixedFunction = 0.0f;
constexpr float kLinearOverSceneCopy = 1.0f;
constexpr float kLinearFixedFunction = 2.0f;
constexpr DWORD kStages[] = {0, 1, 2, 3, 4};

struct Shader
{
    const char* name;
    const BYTE* bytecode;
};

const Shader kComposites[] = {{"plain composite", g_ps_composite_mid},
                              {"noisy composite", g_ps_noisy_composite_mid},
                              {"lit composite", g_ps_lit_composite_mid},
                              {"split composite", g_ps_split_composite_mid}};
constexpr int kCompositeCount = sizeof(kComposites) / sizeof(kComposites[0]);

struct Texel
{
    float r, g, b, a;
};

struct Pass
{
    float blendMode = kLinearOverSceneCopy;
    float glow = 0.0f;
    float rayStrength = 0.0f;
    float debugView = 0.0f;
    bool fixedFunctionBlend = false;
};

struct Frame
{
    bool drawn = false;
    std::vector<unsigned char> bgra;

    int Code(UINT x, UINT y, int channel) const { return bgra[(y * kWidth + x) * 4 + 2 - channel]; }
};

using ColumnColour = D3DCOLOR (*)(UINT);

float RampCode(UINT x)
{
    return kRampFirstCode + (kRampLastCode - kRampFirstCode) * static_cast<float>(x) / (kWidth - 1);
}

float ExpectedRampOnScreen(UINT x, float glow)
{
    const float encoded = RampCode(x) / kCodeMax;
    return kCodeMax * (glow > 0.0f ? PreInvertClientGlow(encoded, glow) : encoded);
}

float QuarterCodeAboveColumn(UINT x)
{
    return std::fmin((static_cast<float>(x) + kQuarterCode) / kCodeMax, 1.0f);
}

int ColumnCode(UINT x, int channel)
{
    const int code = static_cast<int>(x);
    if (channel == 0)
        return code;
    if (channel == 1)
        return kLastCode - code;
    return (code * kBlueCodeStride) & kLastCode;
}

D3DCOLOR ColumnCodes(UINT x)
{
    return D3DCOLOR_XRGB(ColumnCode(x, 0), ColumnCode(x, 1), ColumnCode(x, 2));
}

D3DCOLOR Black(UINT)
{
    return D3DCOLOR_XRGB(0, 0, 0);
}

Texel Grey(float value, float alpha)
{
    return {value, value, value, alpha};
}

Texel LinearRamp(UINT x)
{
    return Grey(std::pow(RampCode(x) / kCodeMax, kDisplayGamma), 1.0f);
}

Texel GammaRamp(UINT x)
{
    return Grey(RampCode(x) / kCodeMax, 1.0f);
}

Texel RayRamp(UINT x)
{
    return Grey(-std::log(1.0f - RampCode(x) / kCodeMax), 1.0f);
}

Texel NoFog(UINT)
{
    return Grey(0.0f, 0.0f);
}

Texel QuarterCodeTransmittance(UINT x)
{
    return Grey(0.0f, 1.0f - QuarterCodeAboveColumn(x));
}

Texel QuarterCodeRadiance(UINT x)
{
    return Grey(std::pow(QuarterCodeAboveColumn(x), kDisplayGamma), 1.0f);
}

struct Fixture
{
    IDirect3DDevice9* device;
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* savedTarget = nullptr;
    IDirect3DSurface9* savedDepth = nullptr;
    IDirect3DVertexBuffer9* savedStream = nullptr;
    UINT savedOffset = 0;
    UINT savedStride = 0;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* readback = nullptr;
    IDirect3DTexture9* depth = nullptr;
    IDirect3DTexture9* fog = nullptr;
    IDirect3DTexture9* rays = nullptr;
    IDirect3DTexture9* scene = nullptr;
    IDirect3DPixelShader9* composites[kCompositeCount] = {};
    IDirect3DPixelShader9* rayComposite = nullptr;

    explicit Fixture(IDirect3DDevice9* value) : device(value) {}

    ~Fixture()
    {
        for (DWORD stage : kStages)
            device->SetTexture(stage, nullptr);
        if (savedTarget)
            device->SetRenderTarget(0, savedTarget);
        device->SetDepthStencilSurface(savedDepth);
        if (state)
            state->Apply();
        device->SetStreamSource(0, savedStream, savedOffset, savedStride);
        IUnknown* resources[] = {state, savedTarget, savedDepth, savedStream, target, readback, depth, fog, rays,
                                 scene, rayComposite};
        for (IUnknown* resource : resources)
            if (resource)
                resource->Release();
        for (IDirect3DPixelShader9* shader : composites)
            if (shader)
                shader->Release();
    }

    bool Create()
    {
        if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &state)) ||
            FAILED(device->GetRenderTarget(0, &savedTarget)) ||
            FAILED(device->GetDepthStencilSurface(&savedDepth)) ||
            FAILED(device->GetStreamSource(0, &savedStream, &savedOffset, &savedStride)) ||
            FAILED(device->CreateRenderTarget(kWidth, kHeight, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                               &target, nullptr)) ||
            FAILED(device->CreateOffscreenPlainSurface(kWidth, kHeight, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                        &readback, nullptr)) ||
            FAILED(device->CreateTexture(kWidth, kHeight, 1, 0, D3DFMT_R32F, D3DPOOL_MANAGED, &depth, nullptr)) ||
            FAILED(device->CreateTexture(kWidth, kHeight, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_MANAGED, &fog,
                                         nullptr)) ||
            FAILED(device->CreateTexture(kWidth, kHeight, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_MANAGED, &rays,
                                         nullptr)) ||
            FAILED(device->CreateTexture(kWidth, kHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &scene,
                                         nullptr)) ||
            FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_ray_composite), &rayComposite)))
            return false;
        for (int i = 0; i < kCompositeCount; ++i)
            if (FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kComposites[i].bytecode),
                                                 &composites[i])))
                return false;
        return FillColumns(depth, [](UINT) { return kSkyDepth; }) && FillColumns(fog, NoFog) &&
               FillColumns(rays, NoFog) && FillColumns(scene, Black);
    }

    template <typename ValueOfColumn>
    static bool FillColumns(IDirect3DTexture9* texture, ValueOfColumn value)
    {
        using Value = decltype(value(0u));
        D3DLOCKED_RECT locked = {};
        if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
            return false;
        for (UINT y = 0; y < kHeight; ++y)
        {
            auto* row = reinterpret_cast<Value*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
            for (UINT x = 0; x < kWidth; ++x)
                row[x] = value(x);
        }
        return SUCCEEDED(texture->UnlockRect(0));
    }

    void ClearColumns(ColumnColour colour)
    {
        for (UINT x = 0; x < kWidth; ++x)
        {
            const D3DRECT column = {static_cast<LONG>(x), 0, static_cast<LONG>(x + 1), static_cast<LONG>(kHeight)};
            device->Clear(1, &column, D3DCLEAR_TARGET, colour(x), 1.0f, 0);
        }
    }

    void BindStages()
    {
        IDirect3DBaseTexture9* const bound[] = {depth, fog, rays, scene, fog};
        for (DWORD stage : kStages)
        {
            device->SetTexture(stage, bound[stage]);
            device->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            device->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            device->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            device->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
            device->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        }
    }

    void SetPassState(const Pass& pass)
    {
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, pass.fixedFunctionBlend ? TRUE : FALSE);
        device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        float constants[kConstants][4] = {};
        constants[0][2] = static_cast<float>(kWidth);
        constants[0][3] = static_cast<float>(kHeight);
        constants[1][0] = 1.0f;
        constants[1][2] = 1.0f / kWidth;
        constants[1][3] = 1.0f / kHeight;
        constants[2][0] = constants[2][1] = 1.0f;
        constants[3][0] = kDepthAtInfiniteViewZ;
        constants[3][1] = kDepthPerInverseViewZ;
        constants[3][2] = kMaxFogDistance;
        constants[3][3] = kDepthAtInfiniteViewZ;
        for (int row = 0; row < 4; ++row)
            constants[4 + row][row] = 1.0f;
        constants[8][0] = static_cast<float>(kWidth);
        constants[8][1] = static_cast<float>(kHeight);
        constants[8][2] = 1.0f / kWidth;
        constants[8][3] = 1.0f / kHeight;
        constants[96][0] = 1.0f;
        constants[96][1] = pass.rayStrength;
        constants[96][2] = pass.debugView;
        constants[96][3] = pass.blendMode;
        constants[97][0] = constants[97][1] = constants[97][2] = 1.0f;
        constants[98][3] = pass.glow;
        device->SetPixelShaderConstantF(0, &constants[0][0], kConstants);
    }

    Frame Draw(IDirect3DPixelShader9* shader, const Pass& pass, ColumnColour background)
    {
        Frame frame;
        const D3DVIEWPORT9 viewport = {0, 0, kWidth, kHeight, 0.0f, 1.0f};
        device->SetDepthStencilSurface(nullptr);
        device->SetRenderTarget(0, target);
        device->SetViewport(&viewport);
        device->SetPixelShader(shader);
        device->SetVertexShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW);
        BindStages();
        SetPassState(pass);
        const float right = kWidth - 0.5f;
        const float bottom = kHeight - 0.5f;
        const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {right, -0.5f, 0, 1}, {-0.5f, bottom, 0, 1},
                                 {right, bottom, 0, 1}};
        const HRESULT begin = device->BeginScene();
        if (SUCCEEDED(begin))
            ClearColumns(background);
        const HRESULT draw =
            SUCCEEDED(begin) ? device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0])) : begin;
        if (SUCCEEDED(begin))
            device->EndScene();
        D3DLOCKED_RECT locked = {};
        if (FAILED(draw) || FAILED(device->GetRenderTargetData(target, readback)) ||
            FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return frame;
        frame.bgra.resize(kPixels * 4);
        for (UINT y = 0; y < kHeight; ++y)
            std::memcpy(&frame.bgra[y * kWidth * 4], static_cast<const BYTE*>(locked.pBits) + y * locked.Pitch,
                        kWidth * 4);
        frame.drawn = SUCCEEDED(readback->UnlockRect());
        return frame;
    }
};

struct RampError
{
    bool drawn = false;
    bool repeats = false;
    bool channelsAgree = true;
    float columnMeanRms = 0.0f;
    float largestColumnMeanError = 0.0f;
    float largestPixelError = 0.0f;
    float meanBias = 0.0f;
};

RampError MeasureRamp(const Frame& frame, const Frame& again, float glow)
{
    RampError e;
    e.drawn = frame.drawn && again.drawn;
    e.repeats = e.drawn && frame.bgra == again.bgra;
    if (!e.drawn)
        return e;
    double squares = 0.0;
    double bias = 0.0;
    for (UINT x = 0; x < kWidth; ++x)
    {
        const float expected = ExpectedRampOnScreen(x, glow);
        double column = 0.0;
        for (UINT y = 0; y < kHeight; ++y)
        {
            const int code = frame.Code(x, y, 0);
            e.channelsAgree = e.channelsAgree && frame.Code(x, y, 1) == code && frame.Code(x, y, 2) == code;
            e.largestPixelError = std::max(e.largestPixelError, std::fabs(static_cast<float>(code) - expected));
            column += code;
        }
        const float columnError = static_cast<float>(column / kHeight) - expected;
        squares += columnError * columnError;
        bias += columnError;
        e.largestColumnMeanError = std::max(e.largestColumnMeanError, std::fabs(columnError));
    }
    e.columnMeanRms = static_cast<float>(std::sqrt(squares / kWidth));
    e.meanBias = static_cast<float>(bias / kWidth);
    return e;
}

bool Dithered(const RampError& e)
{
    return e.drawn && e.repeats && e.channelsAgree && e.columnMeanRms <= kMaxColumnMeanRms &&
           e.largestColumnMeanError <= kMaxColumnMeanError && e.largestPixelError <= kMaxPixelError &&
           std::fabs(e.meanBias) <= kMaxMeanBias;
}

bool DrawsDitheredRamp(Fixture& fixture, IDirect3DPixelShader9* shader, const Pass& pass, const char* what)
{
    const Frame frame = fixture.Draw(shader, pass, &Black);
    const Frame again = fixture.Draw(shader, pass, &Black);
    const RampError e = MeasureRamp(frame, again, pass.glow);
    std::printf("     %s, glow %.2f: column means %.3f rms, largest %.3f codes from the continuous ramp; largest "
                "pixel error %.2f, mean bias %+.3f codes; R = G = B %s, second draw %s\n",
                what, pass.glow, e.columnMeanRms, e.largestColumnMeanError, e.largestPixelError, e.meanBias,
                e.channelsAgree ? "yes" : "no", e.repeats ? "identical" : "different");
    return Dithered(e);
}

template <typename CodeAt>
bool EveryPixelReads(const Frame& frame, CodeAt expected)
{
    if (!frame.drawn)
        return false;
    for (UINT y = 0; y < kHeight; ++y)
        for (UINT x = 0; x < kWidth; ++x)
            for (int channel = 0; channel < 3; ++channel)
                if (frame.Code(x, y, channel) != expected(x, channel))
                    return false;
    return true;
}

bool KeepsColumnCodes(const Frame& frame)
{
    return EveryPixelReads(frame, ColumnCode);
}

bool ReadsColumnIndex(const Frame& frame)
{
    return EveryPixelReads(frame, [](UINT x, int) { return static_cast<int>(x); });
}

void CheckCompositesDitherTheRamp(Fixture& fixture)
{
    const bool filled = Fixture::FillColumns(fixture.fog, LinearRamp) && Fixture::FillColumns(fixture.scene, Black);
    for (int i = 0; i < kCompositeCount; ++i)
    {
        bool dithered = filled;
        for (float glow : {0.0f, kClientGlow})
        {
            Pass pass;
            pass.glow = glow;
            dithered = DrawsDitheredRamp(fixture, fixture.composites[i], pass, kComposites[i].name) && dithered;
        }
        char label[320];
        std::snprintf(label, sizeof(label),
                      "the %s dithers its write over the scene copy: a fog ramp from code 30 to 38 averages to the "
                      "continuous code in every column, no pixel moves by more than half a code beyond rounding, "
                      "R, G and B take one offset and a second draw matches, with and without glow compensation",
                      kComposites[i].name);
        Check(dithered, label);
    }
}

void CheckFixedFunctionBlendDithersTheRamp(Fixture& fixture)
{
    Pass linear;
    linear.blendMode = kLinearFixedFunction;
    linear.fixedFunctionBlend = true;
    Pass gamma = linear;
    gamma.blendMode = kGammaFixedFunction;
    const bool dithered =
        Fixture::FillColumns(fixture.fog, LinearRamp) &&
        DrawsDitheredRamp(fixture, fixture.composites[0], linear, "linear light, fixed-function blend") &&
        Fixture::FillColumns(fixture.fog, GammaRamp) &&
        DrawsDitheredRamp(fixture, fixture.composites[0], gamma, "gamma, fixed-function blend");
    Check(dithered, "without a scene copy the composite dithers the fog's own colour, the source of the "
                    "fixed-function blend: an opaque fog ramp, which leaves none of the scene, averages to the "
                    "continuous code in linear light and in gamma");
}

void CheckLateGodRaysDitherTheRamp(Fixture& fixture)
{
    bool dithered = Fixture::FillColumns(fixture.rays, RayRamp) && Fixture::FillColumns(fixture.scene, Black);
    for (float glow : {0.0f, kClientGlow})
    {
        Pass pass;
        pass.glow = glow;
        pass.rayStrength = kFullRays;
        dithered = DrawsDitheredRamp(fixture, fixture.rayComposite, pass, "late god-ray composite") && dithered;
    }
    Check(dithered, "the late god-ray composite dithers its write: a ray ramp from code 30 to 38 averages to the "
                    "continuous code in every column, with and without glow compensation");
}

void CheckExactCodesStayExact(Fixture& fixture)
{
    bool kept = Fixture::FillColumns(fixture.fog, NoFog) && Fixture::FillColumns(fixture.rays, NoFog) &&
                Fixture::FillColumns(fixture.scene, ColumnCodes);
    Pass overScene;
    overScene.glow = kClientGlow;
    for (IDirect3DPixelShader9* composite : fixture.composites)
        kept = KeepsColumnCodes(fixture.Draw(composite, overScene, &Black)) && kept;
    Pass linear;
    linear.blendMode = kLinearFixedFunction;
    linear.fixedFunctionBlend = true;
    Pass gamma = linear;
    gamma.blendMode = kGammaFixedFunction;
    kept = KeepsColumnCodes(fixture.Draw(fixture.composites[0], linear, &ColumnCodes)) && kept;
    kept = KeepsColumnCodes(fixture.Draw(fixture.composites[0], gamma, &ColumnCodes)) && kept;
    Pass rays;
    rays.glow = kClientGlow;
    rays.rayStrength = kFullRays;
    kept = KeepsColumnCodes(fixture.Draw(fixture.rayComposite, rays, &Black)) && kept;
    Check(kept, "a pixel without fog or rays keeps its exact 8-bit code under the dither, every code 0..255 in R, G "
                "and B, through each composite over the scene copy (with glow compensation), both fixed-function "
                "blends and the late god-ray composite");
}

void CheckDebugViewsStayUndithered(Fixture& fixture)
{
    Pass transmittance;
    transmittance.debugView = kTransmittanceView;
    Pass radiance;
    radiance.debugView = kRadianceView;
    const bool undithered =
        Fixture::FillColumns(fixture.fog, QuarterCodeTransmittance) &&
        ReadsColumnIndex(fixture.Draw(fixture.composites[0], transmittance, &Black)) &&
        Fixture::FillColumns(fixture.fog, QuarterCodeRadiance) &&
        ReadsColumnIndex(fixture.Draw(fixture.composites[0], radiance, &Black));
    Check(undithered, "the transmittance and radiance debug views stay undithered: a value a quarter code above each "
                      "code reads that code at every pixel");
}

void CheckDisplayDither(IDirect3DDevice9* device)
{
    Fixture fixture(device);
    const bool ready = fixture.Create();
    Check(ready, "display dither fixtures created");
    if (!ready)
        return;
    CheckCompositesDitherTheRamp(fixture);
    CheckFixedFunctionBlendDithersTheRamp(fixture);
    CheckLateGodRaysDitherTheRamp(fixture);
    CheckExactCodesStayExact(fixture);
    CheckDebugViewsStayUndithered(fixture);
}
}

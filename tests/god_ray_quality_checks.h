#pragma once

namespace god_ray_quality
{
constexpr UINT kSize = 8;
constexpr float kGlow = 0.65f;
constexpr float kScene[] = {0.45f, 0.55f, 0.65f};
constexpr float kFogColour[] = {0.6f, 0.5f, 0.4f};

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
    IDirect3DTexture9* textures[4] = {};
    IDirect3DPixelShader9* shaders[3] = {};
    BYTE bytes[kSize * kSize][3] = {};

    explicit Fixture(IDirect3DDevice9* value) : device(value) {}

    ~Fixture()
    {
        for (DWORD stage = 0; stage < 4; ++stage)
            device->SetTexture(stage, nullptr);
        device->SetDepthStencilSurface(nullptr);
        if (savedTarget)
            device->SetRenderTarget(0, savedTarget);
        device->SetDepthStencilSurface(savedDepth);
        if (state)
            state->Apply();
        device->SetStreamSource(0, savedStream, savedOffset, savedStride);
        IUnknown* resources[] = {state, savedTarget, savedDepth, savedStream, target, readback,
                                 textures[0], textures[1], textures[2], textures[3],
                                 shaders[0], shaders[1], shaders[2]};
        for (IUnknown* resource : resources)
            if (resource)
                resource->Release();
    }

    bool Fill(UINT stage, const float (&values)[4])
    {
        D3DLOCKED_RECT locked = {};
        if (FAILED(textures[stage]->LockRect(0, &locked, nullptr, 0)))
            return false;
        std::memcpy(locked.pBits, values, sizeof(values));
        return SUCCEEDED(textures[stage]->UnlockRect(0));
    }

    bool Create()
    {
        if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &state)) ||
            FAILED(device->GetRenderTarget(0, &savedTarget)) ||
            FAILED(device->GetDepthStencilSurface(&savedDepth)) ||
            FAILED(device->GetStreamSource(0, &savedStream, &savedOffset, &savedStride)) ||
            FAILED(device->CreateRenderTarget(kSize, kSize, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE,
                                               0, FALSE, &target, nullptr)) ||
            FAILED(device->CreateOffscreenPlainSurface(kSize, kSize, D3DFMT_A8R8G8B8,
                                                        D3DPOOL_SYSTEMMEM, &readback, nullptr)))
            return false;
        for (IDirect3DTexture9*& texture : textures)
            if (FAILED(device->CreateTexture(1, 1, 1, 0, D3DFMT_A32B32G32R32F,
                                               D3DPOOL_MANAGED, &texture, nullptr)))
                return false;
        const BYTE* bytecode[] = {g_ps_composite_low, g_ps_composite_mid, g_ps_composite_high};
        for (int quality = 0; quality < 3; ++quality)
            if (FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(bytecode[quality]),
                                                   &shaders[quality])))
                return false;
        const float depth[] = {1, 0, 0, 0};
        const float rays[] = {1, 1, 1, 1};
        const float scene[] = {kScene[0], kScene[1], kScene[2], 1};
        return Fill(0, depth) && Fill(2, rays) && Fill(3, scene);
    }

    bool Draw(int quality, bool linearLight, float opacity, float strength, float glow, float (&colour)[3])
    {
        const float fog[] = {kFogColour[0] * opacity, kFogColour[1] * opacity,
                             kFogColour[2] * opacity, opacity};
        if (!Fill(1, fog))
            return false;
        device->SetDepthStencilSurface(nullptr);
        device->SetRenderTarget(0, target);
        const D3DVIEWPORT9 viewport = {0, 0, kSize, kSize, 0, 1};
        device->SetViewport(&viewport);
        device->SetPixelShader(shaders[quality]);
        device->SetVertexShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW);
        for (DWORD stage = 0; stage < 4; ++stage)
        {
            device->SetTexture(stage, textures[stage]);
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
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        float constants[99][4] = {};
        constants[0][2] = constants[0][3] = static_cast<float>(kSize);
        constants[1][0] = static_cast<float>(kSize);
        constants[1][2] = constants[1][3] = 1;
        constants[2][0] = constants[2][1] = 1;
        constants[3][0] = 1.0004f;
        constants[3][1] = -0.40016f;
        constants[3][2] = 1000;
        constants[3][3] = 0.94f;
        for (int row = 0; row < 4; ++row)
            constants[4 + row][row] = 1;
        for (int channel = 0; channel < 4; ++channel)
            constants[8][channel] = 1;
        constants[96][0] = 1;
        constants[96][1] = strength;
        constants[96][3] = linearLight ? 1.0f : 3.0f;
        constants[97][0] = 1;
        constants[97][1] = 0.95f;
        constants[97][2] = 0.8f;
        constants[98][3] = glow;
        device->SetPixelShaderConstantF(0, &constants[0][0], 99);
        const float edge = kSize - 0.5f;
        const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {edge, -0.5f, 0, 1},
                                 {-0.5f, edge, 0, 1}, {edge, edge, 0, 1}};
        if (FAILED(device->BeginScene()))
            return false;
        const HRESULT result = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
        device->EndScene();
        D3DLOCKED_RECT locked = {};
        if (FAILED(result) || FAILED(device->GetRenderTargetData(target, readback)) ||
            FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return false;
        const BYTE* const bits = static_cast<const BYTE*>(locked.pBits);
        for (UINT y = 0; y < kSize; ++y)
            for (UINT x = 0; x < kSize; ++x)
                for (int channel = 0; channel < 3; ++channel)
                    bytes[y * kSize + x][channel] = bits[y * locked.Pitch + x * 4 + 2 - channel];
        for (int channel = 0; channel < 3; ++channel)
            colour[channel] = bytes[(kSize / 2) * kSize + kSize / 2][channel] / 255.0f;
        return SUCCEEDED(readback->UnlockRect());
    }
};

float AfterGlow(float colour, float glow)
{
    return colour + glow * colour * colour;
}

float FogOnly(int channel, bool linearLight, float opacity, float glow)
{
    if (!linearLight)
        return kScene[channel] * (1 - opacity) + kFogColour[channel] * opacity;
    const float scene = std::pow(kScene[channel], 2.2f);
    const float linear = scene * (1 - opacity) + kFogColour[channel] * opacity;
    const float knee = (std::max)(0.8f, scene);
    const float rolled = linear <= knee ? linear : knee + (1 - knee) * (1 - std::exp(-(linear - knee) / (1 - knee)));
    const float colour = std::pow(rolled, 1.0f / 2.2f);
    const float compensated = 2 * colour / (1 + std::sqrt(1 + 4 * glow * colour));
    return colour + (compensated - colour) * opacity;
}

void CheckMode(Fixture& fixture, bool linearLight)
{
    const float strengths[] = {0, 0.2f, 1, 4};
    for (int quality = 0; quality < 3; ++quality)
        for (float opacity : {0.0f, 0.015f})
        {
            float colours[4][2][3] = {};
            bool drawn = true;
            for (int strength = 0; strength < 4; ++strength)
                for (int glow = 0; glow < 2; ++glow)
                    drawn = fixture.Draw(quality, linearLight, opacity, strengths[strength], glow == 0 ? 0 : kGlow,
                                           colours[strength][glow]) && drawn;
            bool bounded = drawn;
            bool continuous = drawn;
            bool visible = drawn;
            bool compensated = drawn;
            bool unchanged = drawn;
            float peak = 0;
            for (int channel = 0; channel < 3; ++channel)
            {
                for (int glow = 0; glow < 2; ++glow)
                {
                    const float expected = FogOnly(channel, linearLight, opacity, glow == 0 ? 0 : kGlow);
                    unchanged = unchanged && std::fabs(colours[0][glow][channel] - expected) <= 2.0f / 255.0f;
                    for (int strength = 1; strength < 4; ++strength)
                    {
                        const float onScreen = AfterGlow(colours[strength][glow][channel], glow == 0 ? 0 : kGlow);
                        const float previous = AfterGlow(colours[strength - 1][glow][channel], glow == 0 ? 0 : kGlow);
                        peak = (std::max)(peak, onScreen);
                        bounded = bounded && onScreen <= 1.01f;
                        continuous = continuous && onScreen + 1.0f / 255.0f >= previous;
                    }
                }
                visible = visible && AfterGlow(colours[1][1][channel], kGlow) >
                                       AfterGlow(colours[0][1][channel], kGlow) + 1.0f / 255.0f;
                compensated = compensated && colours[3][1][channel] < colours[3][0][channel] - 0.1f;
            }
            char label[144];
            const char* medium = opacity == 0 ? "zero fog" : "thin fog";
            const char* mode = linearLight ? "linear" : "gamma";
            std::snprintf(label, sizeof(label), "god rays quality %d %s %s bounded after client glow (peak %.3f)",
                          quality + 1, mode, medium, peak);
            Check(bounded, label);
            std::snprintf(label, sizeof(label), "god rays quality %d %s %s has visible gradual strength response",
                          quality + 1, mode, medium);
            Check(visible && continuous, label);
            std::snprintf(label, sizeof(label), "god rays quality %d %s %s compensates ray highlights for client glow",
                          quality + 1, mode, medium);
            Check(compensated, label);
            std::snprintf(label, sizeof(label), "god rays quality %d %s %s preserves zero-strength fog-only output",
                          quality + 1, mode, medium);
            Check(unchanged, label);
        }
}

constexpr BYTE kExactScene[] = {115, 140, 166};
constexpr float kDitheredOpacity = 0.012f;
constexpr double kMeanToleranceLevels = 0.2;

double ExpectedLevels(int channel, float opacity)
{
    const double scene = kExactScene[channel];
    return scene + (kFogColour[channel] - scene / 255.0) * opacity * 255.0;
}

void CheckFogOverTheSceneIsDithered(Fixture& fixture, bool linearLight)
{
    const float scene[] = {kExactScene[0] / 255.0f, kExactScene[1] / 255.0f, kExactScene[2] / 255.0f, 1};
    float colour[3] = {};
    const bool clear = fixture.Fill(3, scene) && fixture.Draw(0, linearLight, 0.0f, 0.0f, 0.0f, colour);
    bool untouched = clear;
    for (int pixel = 0; pixel < kSize * kSize; ++pixel)
        for (int channel = 0; channel < 3; ++channel)
            untouched = untouched && fixture.bytes[pixel][channel] == kExactScene[channel];
    const bool thin = fixture.Draw(0, linearLight, kDitheredOpacity, 0.0f, 0.0f, colour);
    double mean[3] = {};
    bool varied = false;
    for (int pixel = 0; pixel < kSize * kSize; ++pixel)
    {
        varied = varied || fixture.bytes[pixel][0] != fixture.bytes[0][0];
        for (int channel = 0; channel < 3; ++channel)
            mean[channel] += fixture.bytes[pixel][channel] / static_cast<double>(kSize * kSize);
    }
    bool unbiased = thin;
    for (int channel = 0; channel < 3; ++channel)
        unbiased = unbiased && std::fabs(mean[channel] - ExpectedLevels(channel, kDitheredOpacity)) <
                                   kMeanToleranceLevels;
    const float original[] = {kScene[0], kScene[1], kScene[2], 1};
    fixture.Fill(3, original);
    char label[160];
    std::snprintf(label, sizeof(label), "%s composite leaves a fog-free scene exactly as it was",
                  linearLight ? "linear" : "gamma");
    Check(untouched, label);
    if (linearLight)
        return;
    std::snprintf(label, sizeof(label),
                  "%s composite dithers a sub-level fog gradient to its true mean (%.2f %.2f %.2f levels, expected "
                  "%.2f %.2f %.2f)",
                  linearLight ? "linear" : "gamma", mean[0], mean[1], mean[2], ExpectedLevels(0, kDitheredOpacity),
                  ExpectedLevels(1, kDitheredOpacity), ExpectedLevels(2, kDitheredOpacity));
    Check(unbiased && varied, label);
}

void CheckGodRays(IDirect3DDevice9* device, bool includeGamma = true)
{
    Fixture fixture(device);
    const bool ready = fixture.Create();
    Check(ready, "sun-facing god-ray composite fixtures created");
    if (!ready)
        return;
    CheckMode(fixture, true);
    CheckFogOverTheSceneIsDithered(fixture, true);
    if (includeGamma)
    {
        CheckMode(fixture, false);
        CheckFogOverTheSceneIsDithered(fixture, false);
    }
}
}

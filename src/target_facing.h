#pragma once

#include "client_code.h"

#include <d3d9.h>

#include <cstddef>
#include <cstdint>

constexpr float kTargetArrowBoxScale = 1.35f;
constexpr float kChevronTip = 1.15f;
constexpr float kChevronNotch = 1.01f;
constexpr float kChevronBack = 0.93f;
constexpr float kChevronHalfWidth = 0.16f;
constexpr float kChevronNotchBack = 0.89f;
constexpr float kChevronNotchHalfWidth = 0.24f;
constexpr float kChevronOpacity = 0.9f;
constexpr float kChevronEdgeTexels = 5.0f;
constexpr float kChevronTipBrightness = 0.45f;
constexpr int kChevronTextureSize = 512;
constexpr int kAabbFloats = 6;
constexpr int kMatrixFloats = 16;

namespace engine
{
constexpr uintptr_t kTargetCircleRenderSite = 0x00725A8A;
constexpr uintptr_t kProjectedTextureRender = 0x007E4370;
constexpr uintptr_t kUnitRenderFacing = 0x007156A0;

const ClientCodeGuard* TargetFacingGuards(size_t& count);
bool TargetFacingCodeMatches(const ClientCodeView& code);
}

struct ProjectedTextureArgs
{
    const float* box;
    const uint32_t* color;
    const float* rotation;
    float fade;
    uint32_t triangleFlags;
    uint32_t flags;
    float polygonOffset;
};

using ProjectedTextureRenderFn = void(__cdecl*)(const float* box, const uint32_t* color, const float* rotation,
                                                float fade, uint32_t triangleFlags, uint32_t flags,
                                                float polygonOffset);
using UnitRenderFacingFn = float(__thiscall*)(uintptr_t unit);

void TargetArrowBox(const float circleBox[kAabbFloats], float out[kAabbFloats]);
void TargetArrowRotation(float facing, float out[kMatrixFloats]);
float ChevronCoverage(float u, float v, float texel);
float ChevronBrightness(float v);
void FillChevronLevel(uint32_t* texels, int size, size_t pitchBytes);

class ChevronTexture
{
public:
    IDirect3DTexture9* Get(IDirect3DDevice9* dev);
    void Release();

private:
    IDirect3DTexture9* m_texture = nullptr;
    bool m_failed = false;
};

class StageTextureSwap
{
public:
    bool Begin(IDirect3DDevice9* dev, IDirect3DBaseTexture9* replacement);
    void End(IDirect3DDevice9* dev, IDirect3DBaseTexture9*& restored);
    bool Active() const { return m_active; }
    bool HoldTexture(DWORD stage, IDirect3DBaseTexture9* texture);
    bool HoldSamplerState(DWORD stage, D3DSAMPLERSTATETYPE type, DWORD value);

private:
    bool m_active = false;
    IDirect3DBaseTexture9* m_held = nullptr;
    DWORD m_addressU = D3DTADDRESS_WRAP;
    DWORD m_addressV = D3DTADDRESS_WRAP;
};

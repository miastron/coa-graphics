#include "target_facing.h"

#include "log.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kTextureCentre = 0.5f;
constexpr float kHalfCoverage = 0.5f;
constexpr float kByteMax = 255.0f;
constexpr int kAlphaShift = 24;
constexpr uint32_t kWhiteTexel = (1u << kAlphaShift) - 1;
constexpr UINT kAllLevels = 0;

const ClientCodeGuard kTargetFacingCode[] = {
    {"target circle unit register", 0x00725990, 3, {0x56, 0x8B, 0xF1}},
    {"target circle render states", 0x00725A56, 7, {0x8B, 0xCE, 0xE8, 0x53, 0xF4, 0x01, 0x00}},
    {"target circle render arguments", 0x00725A63, 16,
     {0xD9, 0x05, 0xD8, 0x98, 0x9F, 0x00, 0x51, 0xD9, 0x1C, 0x24, 0x6A, 0x01, 0xD9, 0x05, 0xC4, 0x2E}},
    {"target circle render argument pointers", 0x00725A73, 16,
     {0x9E, 0x00, 0x68, 0x22, 0x01, 0x20, 0x00, 0x51, 0xD9, 0x1C, 0x24, 0x8D, 0x55, 0x84, 0x52, 0x8D}},
    {"target circle render box", 0x00725A83, 7, {0x45, 0xF4, 0x50, 0x8D, 0x4D, 0xDC, 0x51}},
    {"target circle render cleanup", 0x00725A8F, 3, {0x83, 0xC4, 0x1C}},
    {"unit render facing", 0x007156A0, 16,
     {0xD9, 0x81, 0x94, 0x0A, 0x00, 0x00, 0x51, 0x81, 0xC1, 0x88, 0x07, 0x00, 0x00, 0xD9, 0x1C, 0x24}},
    {"unit render facing passenger call", 0x007156B0, 6, {0xE8, 0xEB, 0xEB, 0xDD, 0xFF, 0xC3}},
};

struct Point
{
    float s;
    float t;
};

float Cross(Point a, Point b)
{
    return a.s * b.t - a.t * b.s;
}

float EdgeDistance(Point p, Point a, Point b, float orientation)
{
    const Point edge = {b.s - a.s, b.t - a.t};
    const Point offset = {p.s - a.s, p.t - a.t};
    return -orientation * Cross(edge, offset) / std::sqrt(edge.s * edge.s + edge.t * edge.t);
}

float TriangleDistance(Point p, Point a, Point b, Point c)
{
    const float orientation = Cross({b.s - a.s, b.t - a.t}, {c.s - a.s, c.t - a.t}) > 0.0f ? 1.0f : -1.0f;
    return std::max({EdgeDistance(p, a, b, orientation), EdgeDistance(p, b, c, orientation),
                     EdgeDistance(p, c, a, orientation)});
}

float ChevronDistance(Point p)
{
    const Point tip = {0.0f, kChevronTip};
    const Point notch = {0.0f, kChevronNotch};
    const Point right = {kChevronHalfWidth, kChevronBack};
    const Point left = {-kChevronHalfWidth, kChevronBack};
    const Point notchRight = {kChevronNotchHalfWidth, kChevronNotchBack};
    const Point notchLeft = {-kChevronNotchHalfWidth, kChevronNotchBack};
    return std::max(TriangleDistance(p, tip, right, left), -TriangleDistance(p, notch, notchRight, notchLeft));
}

float Smoothstep(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

bool FillLevel(IDirect3DTexture9* texture, UINT level)
{
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT locked;
    if (FAILED(texture->GetLevelDesc(level, &desc)) || FAILED(texture->LockRect(level, &locked, nullptr, 0)))
        return false;
    FillChevronLevel(static_cast<uint32_t*>(locked.pBits), static_cast<int>(desc.Width),
                     static_cast<size_t>(locked.Pitch));
    texture->UnlockRect(level);
    return true;
}
}

namespace engine
{
const ClientCodeGuard* TargetFacingGuards(size_t& count)
{
    count = sizeof(kTargetFacingCode) / sizeof(kTargetFacingCode[0]);
    return kTargetFacingCode;
}

bool TargetFacingCodeMatches(const ClientCodeView& code)
{
    for (const ClientCodeGuard& guard : kTargetFacingCode)
        if (!ClientGuardMatches(code, guard))
        {
            VF_LOG_ERROR("target arrow: the client's %s at 0x%08X differs from the 12340 client; the target circle "
                         "has no arrow",
                         guard.name, static_cast<unsigned>(guard.address));
            return false;
        }
    return true;
}
}

void TargetArrowBox(const float circleBox[kAabbFloats], float out[kAabbFloats])
{
    for (int axis = 0; axis < 2; ++axis)
    {
        const float centre = (circleBox[axis] + circleBox[axis + 3]) * 0.5f;
        const float half = (circleBox[axis + 3] - circleBox[axis]) * 0.5f * kTargetArrowBoxScale;
        out[axis] = centre - half;
        out[axis + 3] = centre + half;
    }
    out[2] = circleBox[2];
    out[5] = circleBox[5];
}

void TargetArrowRotation(float facing, float out[kMatrixFloats])
{
    const float c = std::cos(-facing);
    const float s = std::sin(-facing);
    const float rotation[kMatrixFloats] = {c, s, 0.0f, 0.0f, -s, c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                           0.0f, 0.0f, 0.0f, 1.0f};
    std::copy(rotation, rotation + kMatrixFloats, out);
}

float ChevronCoverage(float u, float v, float texel)
{
    const float radii = 2.0f * kTargetArrowBoxScale;
    const Point p = {(u - kTextureCentre) * radii, (kTextureCentre - v) * radii};
    const float ramp = kChevronEdgeTexels * texel * radii;
    return Smoothstep(std::clamp(kHalfCoverage - ChevronDistance(p) / ramp, 0.0f, 1.0f));
}

float ChevronBrightness(float v)
{
    const float radius = (kTextureCentre - v) * 2.0f * kTargetArrowBoxScale;
    const float along = std::clamp((radius - kChevronBack) / (kChevronTip - kChevronBack), 0.0f, 1.0f);
    return 1.0f + (kChevronTipBrightness - 1.0f) * along;
}

void FillChevronLevel(uint32_t* texels, int size, size_t pitchBytes)
{
    const float texel = 1.0f / static_cast<float>(size);
    for (int y = 0; y < size; ++y)
    {
        auto* row = reinterpret_cast<uint32_t*>(reinterpret_cast<unsigned char*>(texels) + y * pitchBytes);
        for (int x = 0; x < size; ++x)
        {
            const bool edge = x == 0 || y == 0 || x == size - 1 || y == size - 1;
            const float v = (y + 0.5f) * texel;
            const float coverage = edge ? 0.0f : ChevronCoverage((x + 0.5f) * texel, v, texel);
            const float opacity = kChevronOpacity * ChevronBrightness(v);
            const auto alpha = static_cast<uint32_t>(std::lround(coverage * opacity * kByteMax));
            row[x] = (alpha << kAlphaShift) | kWhiteTexel;
        }
    }
}

IDirect3DTexture9* ChevronTexture::Get(IDirect3DDevice9* dev)
{
    if (m_texture || m_failed)
        return m_texture;
    IDirect3DTexture9* texture = nullptr;
    if (FAILED(dev->CreateTexture(kChevronTextureSize, kChevronTextureSize, kAllLevels, 0, D3DFMT_A8R8G8B8,
                                  D3DPOOL_MANAGED, &texture, nullptr)))
    {
        m_failed = true;
        VF_LOG_ERROR("target arrow: the arrow texture could not be created; the target circle has no arrow");
        return nullptr;
    }
    for (UINT level = 0; level < texture->GetLevelCount(); ++level)
        if (!FillLevel(texture, level))
        {
            texture->Release();
            m_failed = true;
            VF_LOG_ERROR("target arrow: the arrow texture could not be filled; the target circle has no arrow");
            return nullptr;
        }
    m_texture = texture;
    return m_texture;
}

void ChevronTexture::Release()
{
    if (m_texture)
        m_texture->Release();
    m_texture = nullptr;
    m_failed = false;
}

bool StageTextureSwap::Begin(IDirect3DDevice9* dev, IDirect3DBaseTexture9* replacement)
{
    if (m_active || !replacement || FAILED(dev->GetTexture(0, &m_held)))
        return false;
    dev->GetSamplerState(0, D3DSAMP_ADDRESSU, &m_addressU);
    dev->GetSamplerState(0, D3DSAMP_ADDRESSV, &m_addressV);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetTexture(0, replacement);
    m_active = true;
    return true;
}

void StageTextureSwap::End(IDirect3DDevice9* dev, IDirect3DBaseTexture9*& restored)
{
    restored = m_held;
    m_held = nullptr;
    if (!m_active)
        return;
    m_active = false;
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, m_addressU);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, m_addressV);
}

bool StageTextureSwap::HoldTexture(DWORD stage, IDirect3DBaseTexture9* texture)
{
    if (!m_active || stage != 0)
        return false;
    if (texture)
        texture->AddRef();
    if (m_held)
        m_held->Release();
    m_held = texture;
    return true;
}

bool StageTextureSwap::HoldSamplerState(DWORD stage, D3DSAMPLERSTATETYPE type, DWORD value)
{
    if (!m_active || stage != 0)
        return false;
    if (type == D3DSAMP_ADDRESSU)
        m_addressU = value;
    else if (type == D3DSAMP_ADDRESSV)
        m_addressV = value;
    else
        return false;
    return true;
}

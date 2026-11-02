#include "snow_surface.h"

#include "d3d_util.h"
#include "log.h"

#include <cstring>

namespace
{
constexpr DWORD kVertexSampler = D3DVERTEXTEXTURESAMPLER0;

void Sample(IDirect3DDevice9* dev, DWORD sampler, D3DTEXTUREFILTERTYPE mip)
{
    dev->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    dev->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    dev->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(sampler, D3DSAMP_MIPFILTER, mip);
    dev->SetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, FALSE);
}
}

void SnowSurface::NoteTerrainShader(IDirect3DVertexShader9* shader)
{
    if (shader)
        m_terrainShaders.insert(shader);
}

void SnowSurface::NotePixelShader(IDirect3DPixelShader9* shader, IDirect3DPixelShader9* depthTwin)
{
    auto found = m_depthTwins.find(shader);
    if (found != m_depthTwins.end())
    {
        SafeRelease(found->second);
        m_depthTwins.erase(found);
    }
    if (shader && depthTwin)
        m_depthTwins.emplace(shader, depthTwin);
}

IDirect3DPixelShader9* SnowSurface::DepthTwin(IDirect3DPixelShader9* shader) const
{
    auto found = m_depthTwins.find(shader);
    return found != m_depthTwins.end() ? found->second : nullptr;
}

bool SnowSurface::Create(IDirect3DDevice9* dev)
{
    if (SUCCEEDED(dev->CreateTexture(kSnowMapTexels, kSnowMapTexels, 0, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8,
                                     D3DPOOL_MANAGED, &m_texture, nullptr)))
    {
        m_mipmaps = true;
        return true;
    }
    m_texture = nullptr;
    if (SUCCEEDED(dev->CreateTexture(kSnowMapTexels, kSnowMapTexels, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                     &m_texture, nullptr)))
    {
        m_mipmaps = false;
        VF_LOG_INFO("snow: the snow map has no mipmaps; distant snow edges may shimmer");
        return true;
    }
    m_texture = nullptr;
    return false;
}

bool SnowSurface::Upload(IDirect3DDevice9* dev, const SnowMap& map)
{
    if (!m_texture && !m_creationFailed && !Create(dev))
    {
        m_creationFailed = true;
        VF_LOG_ERROR("snow: the %dx%d snow map texture could not be created; the terrain stays bare", kSnowMapTexels,
                     kSnowMapTexels);
    }
    if (!m_texture)
        return false;
    if (m_uploaded && map.Generation() == m_generation)
        return true;
    D3DLOCKED_RECT locked = {};
    if (FAILED(m_texture->LockRect(0, &locked, nullptr, 0)))
        return false;
    const uint32_t* texels = map.Texels();
    for (int row = 0; row < kSnowMapTexels; ++row)
        std::memcpy(static_cast<unsigned char*>(locked.pBits) + row * locked.Pitch, texels + row * kSnowMapTexels,
                    sizeof(uint32_t) * kSnowMapTexels);
    if (FAILED(m_texture->UnlockRect(0)))
        return false;
    if (m_mipmaps)
        m_texture->GenerateMipSubLevels();
    m_generation = map.Generation();
    m_uploaded = true;
    ++m_uploads;
    return true;
}

bool SnowSurface::UploadTrenches(IDirect3DDevice9* dev, TrenchMap& map)
{
    if (!m_trenches && !m_trenchFailed &&
        FAILED(dev->CreateTexture(kTrenchTexels, kTrenchTexels, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_trenches,
                                  nullptr)))
    {
        m_trenches = nullptr;
        m_trenchFailed = true;
        VF_LOG_ERROR("snow: the %dx%d trench texture could not be created; the snow keeps no trenches", kTrenchTexels,
                     kTrenchTexels);
    }
    if (!m_trenches)
        return false;
    if (map.Generation() == m_trenchGeneration)
        return true;
    D3DLOCKED_RECT locked = {};
    if (FAILED(m_trenches->LockRect(0, &locked, nullptr, 0)))
        return false;
    const uint32_t* texels = map.Texels();
    for (int row = 0; row < kTrenchTexels; ++row)
        std::memcpy(static_cast<unsigned char*>(locked.pBits) + row * locked.Pitch, texels + row * kTrenchTexels,
                    sizeof(uint32_t) * kTrenchTexels);
    if (FAILED(m_trenches->UnlockRect(0)))
        return false;
    m_trenchGeneration = map.Generation();
    return true;
}

void SnowSurface::Bind(IDirect3DDevice9* dev, const SnowFrame& frame) const
{
    IDirect3DTexture9* texture = Ready() ? m_texture : nullptr;
    dev->SetTexture(kVertexSampler, texture);
    dev->SetTexture(kSnowPixelSampler, texture);
    dev->SetTexture(kTrenchPixelSampler, m_trenches);
    Sample(dev, kVertexSampler, D3DTEXF_NONE);
    Sample(dev, kSnowPixelSampler, m_mipmaps ? D3DTEXF_LINEAR : D3DTEXF_NONE);
    Sample(dev, kTrenchPixelSampler, D3DTEXF_NONE);
    dev->SetVertexShaderConstantF(kSnowVertexRegister, &frame.vertex.registers[0][0], kSnowVertexRegisterCount);
    dev->SetPixelShaderConstantF(kSnowPixelRegister, &frame.pixel.registers[0][0], kSnowPixelRegisterCount);
    dev->SetPixelShaderConstantF(kTrenchPixelRegister, &frame.trench.registers[0][0], kTrenchPixelRegisterCount);
}

bool SnowSurface::StillBound(IDirect3DDevice9* dev, const SnowFrame& frame) const
{
    float vertex[kSnowVertexRegisterCount][4];
    if (FAILED(dev->GetVertexShaderConstantF(kSnowVertexRegister, &vertex[0][0], kSnowVertexRegisterCount)) ||
        std::memcmp(vertex, frame.vertex.registers, sizeof(vertex)) != 0)
        return false;
    IDirect3DBaseTexture9* bound = nullptr;
    if (FAILED(dev->GetTexture(kVertexSampler, &bound)))
        return false;
    if (bound)
        bound->Release();
    return bound == (Ready() ? m_texture : nullptr);
}

void SnowSurface::KeepBoundForDraw(IDirect3DDevice9* dev, const SnowFrame& frame)
{
    ++m_terrainDraws;
    if (StillBound(dev, frame))
        return;
    ++m_reboundDraws;
    Bind(dev, frame);
}

void SnowSurface::Release()
{
    SafeRelease(m_texture);
    SafeRelease(m_trenches);
    m_trenchGeneration = ~0u;
    m_trenchFailed = false;
    m_terrainShaders.clear();
    for (auto& twin : m_depthTwins)
        SafeRelease(twin.second);
    m_depthTwins.clear();
    m_uploaded = false;
    m_creationFailed = false;
}

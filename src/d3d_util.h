#pragma once

#include <d3d9.h>

template <typename T>
void SafeRelease(T*& p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}

inline bool CreateTarget(IDirect3DDevice9* dev, UINT width, UINT height, UINT levels, D3DFORMAT format,
                         IDirect3DTexture9** out)
{
    return SUCCEEDED(
        dev->CreateTexture(width, height, levels, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, out, nullptr));
}

inline bool CreateTarget(IDirect3DDevice9* dev, UINT width, UINT height, D3DFORMAT format, IDirect3DTexture9** out)
{
    return CreateTarget(dev, width, height, 1, format, out);
}

inline void SetTarget(IDirect3DDevice9* dev, IDirect3DTexture9* texture)
{
    IDirect3DSurface9* surface = nullptr;
    if (SUCCEEDED(texture->GetSurfaceLevel(0, &surface)))
    {
        dev->SetRenderTarget(0, surface);
        surface->Release();
    }
}

inline void SetPointClampSampler(IDirect3DDevice9* dev, DWORD stage, D3DTEXTUREFILTERTYPE mipFilter)
{
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, mipFilter);
    dev->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
    dev->SetSamplerState(stage, D3DSAMP_MAXMIPLEVEL, 0);
    dev->SetSamplerState(stage, D3DSAMP_MIPMAPLODBIAS, 0);
}

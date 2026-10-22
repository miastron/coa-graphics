#pragma once

#include <d3d9.h>

inline constexpr float kFullscreenTriangleLow = -1.5f;
inline constexpr float kFullscreenTriangleHigh = 4.5f;
inline constexpr float kFullscreenTriangle[3][4] = {
    {kFullscreenTriangleLow, kFullscreenTriangleLow, 0.0f, 1.0f},
    {kFullscreenTriangleLow, kFullscreenTriangleHigh, 0.0f, 1.0f},
    {kFullscreenTriangleHigh, kFullscreenTriangleLow, 0.0f, 1.0f},
};

inline HRESULT DrawFullscreenTriangle(IDirect3DDevice9* dev)
{
    return dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, kFullscreenTriangle, sizeof(kFullscreenTriangle[0]));
}

inline HRESULT CreateFullscreenVertexDeclaration(IDirect3DDevice9* dev, IDirect3DVertexDeclaration9** declaration)
{
    static const D3DVERTEXELEMENT9 kElements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END(),
    };
    return dev->CreateVertexDeclaration(kElements, declaration);
}

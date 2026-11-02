#pragma once

#include "snow_cover.h"

#include <d3d9.h>

#include <unordered_map>
#include <unordered_set>

class SnowSurface
{
public:
    void NoteTerrainShader(IDirect3DVertexShader9* shader);
    bool Carries(IDirect3DVertexShader9* shader) const { return shader && m_terrainShaders.count(shader) != 0; }
    void NotePixelShader(IDirect3DPixelShader9* shader, IDirect3DPixelShader9* depthTwin);
    IDirect3DPixelShader9* DepthTwin(IDirect3DPixelShader9* shader) const;
    bool Upload(IDirect3DDevice9* dev, const SnowMap& map);
    bool UploadTrenches(IDirect3DDevice9* dev, TrenchMap& map);
    void Bind(IDirect3DDevice9* dev, const SnowFrame& frame) const;
    bool StillBound(IDirect3DDevice9* dev, const SnowFrame& frame) const;
    void KeepBoundForDraw(IDirect3DDevice9* dev, const SnowFrame& frame);
    unsigned TerrainDraws() const { return m_terrainDraws; }
    unsigned ReboundDraws() const { return m_reboundDraws; }
    void Release();
    bool Ready() const { return m_texture && m_uploaded; }
    unsigned Uploads() const { return m_uploads; }
    size_t TerrainShaders() const { return m_terrainShaders.size(); }

private:
    bool Create(IDirect3DDevice9* dev);

    IDirect3DTexture9* m_texture = nullptr;
    IDirect3DTexture9* m_trenches = nullptr;
    unsigned m_trenchGeneration = ~0u;
    bool m_trenchFailed = false;
    std::unordered_set<IDirect3DVertexShader9*> m_terrainShaders;
    std::unordered_map<IDirect3DPixelShader9*, IDirect3DPixelShader9*> m_depthTwins;
    unsigned m_generation = 0;
    unsigned m_uploads = 0;
    unsigned m_terrainDraws = 0;
    unsigned m_reboundDraws = 0;
    bool m_uploaded = false;
    bool m_creationFailed = false;
    bool m_mipmaps = false;
};

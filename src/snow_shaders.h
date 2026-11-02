/*
 * Copyright (C) 2026 Morfium-G
 * The terrain shader rewrite (world position handed to the pixel shader, the stock fog ending as the anchor) derives
 * from seyris-living-azeroth (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU General
 * Public License version 3.
 */
#pragma once

#include "grass_wind.h"

#include <windows.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

enum class SnowShaderKind
{
    Vertex,
    Pixel,
    DepthPixel,
};

bool EditTerrainVertexSource(std::string& source, std::string& why);
bool EditTerrainPixelSource(std::string& source, std::string& why, bool writeDepth);

class SnowShaderRewriter
{
public:
    void UseCompiler(const ShaderCompiler& compiler);
    void Enable(bool enabled) { m_enabled = enabled; }
    bool Enabled() const { return m_enabled; }
    const DWORD* Rewrite(const DWORD* code, SnowShaderKind kind);
    bool Paired() const { return m_vertexInjected > 0 && m_vertexFailed == 0; }
    unsigned VertexInjected() const { return m_vertexInjected; }
    unsigned VertexFailed() const { return m_vertexFailed; }
    unsigned PixelInjected() const { return m_pixelInjected; }
    unsigned PixelLeft() const { return m_pixelLeft; }

private:
    bool CompilerReady();
    std::vector<DWORD> Build(const DWORD* code, size_t tokens, SnowShaderKind kind, bool& left);

    bool m_enabled = false;
    bool m_compilerLoaded = false;
    ShaderCompiler m_compiler;
    std::map<uint64_t, std::vector<DWORD>> m_cache;
    unsigned m_vertexInjected = 0;
    unsigned m_vertexFailed = 0;
    unsigned m_pixelInjected = 0;
    unsigned m_pixelLeft = 0;
};

SnowShaderRewriter& GlobalSnowShaders();

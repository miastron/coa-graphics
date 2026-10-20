/*
 * Copyright (C) 2026 WarcraftXL
 * Derived from wxl-grasswind (https://github.com/WarcraftXL/wxl-grasswind), licensed under the GNU General Public
 * License version 3; see LICENSE.
 */
#pragma once

#include "client_code.h"
#include "config.h"

#include <windows.h>
#include <d3dcompiler.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

struct GrassWindSettings
{
    bool wind;
    float directionDeg;
    float speed;
    float amplitude;
    float wavelength;
    float crossAmplitude;
    float crossWavelength;
    float crossAngleDeg;
    float lean;
    float variance;
    float distanceFade;
    float anchor;
    bool parting;
    float radius;
    float force;
    float edgeForce;
    float coneHeight;
    float minBladeHeight;
};

GrassWindSettings GrassWindSettingsFrom(const Config& config);

constexpr size_t kGrassConstantRegisters = 8;

struct GrassConstants
{
    float registers[kGrassConstantRegisters][4];
};

GrassConstants GrassFrameConstants(const GrassWindSettings& settings, const float camera[3], const float* player,
                                   uint32_t tickMs);

namespace engine
{
constexpr uintptr_t kInitializeShaderConstants = 0x007B15D0;
constexpr uintptr_t kInitializeShaderConstantsSite = 0x007B2E25;
constexpr uintptr_t kDetailDoodadShaderConstants = 0x00D1C518;
constexpr size_t kDetailDoodadFirstFreeRegister = 14;
constexpr size_t kDetailDoodadUploadedRegisters = 23;
constexpr size_t kRegisterBytes = 4 * sizeof(float);

const ClientCodeGuard* GrassWindGuards(size_t& count);
bool GrassWindCodeMatches(const ClientCodeView& code);
}

using ShaderDisassembleFn = HRESULT(WINAPI*)(LPCVOID, SIZE_T, UINT, LPCSTR, ID3DBlob**);
using ShaderAssembleFn = HRESULT(WINAPI*)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, UINT,
                                          ID3DBlob**, ID3DBlob**);

struct ShaderCompiler
{
    ShaderDisassembleFn disassemble = nullptr;
    ShaderAssembleFn assemble = nullptr;
};

ShaderCompiler LoadSystemShaderCompiler();
size_t ShaderTokenCount(const DWORD* code);

class GrassShaderRewriter
{
public:
    void UseCompiler(const ShaderCompiler& compiler);
    void Enable(bool enabled) { m_enabled = enabled; }
    bool Enabled() const { return m_enabled; }
    const DWORD* Rewrite(const DWORD* code);
    unsigned Injected() const { return m_injected; }
    unsigned Failed() const { return m_failed; }
    unsigned Foreign() const { return m_foreign; }

private:
    bool CompilerReady();
    std::vector<DWORD> Build(const DWORD* code, size_t tokens, bool& foreign);

    bool m_enabled = false;
    bool m_compilerLoaded = false;
    ShaderCompiler m_compiler;
    std::map<uint64_t, std::vector<DWORD>> m_cache;
    unsigned m_injected = 0;
    unsigned m_failed = 0;
    unsigned m_foreign = 0;
};

class GrassWind
{
public:
    void Configure(const GrassWindSettings& settings) { m_settings = settings; }
    const GrassWindSettings& Settings() const { return m_settings; }
    GrassShaderRewriter& Shaders() { return m_shaders; }

private:
    GrassWindSettings m_settings = GrassWindSettingsFrom(Config());
    GrassShaderRewriter m_shaders;
};

GrassWind& GlobalGrassWind();

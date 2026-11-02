/*
 * Copyright (C) 2026 WarcraftXL
 * Derived from wxl-grasswind (https://github.com/WarcraftXL/wxl-grasswind), licensed under the GNU General Public
 * License version 3; see LICENSE.
 *
 * Copyright (C) 2026 Morfium-G
 * The wind grid, the unit parting and the motion block derive from seyris-living-azeroth
 * (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU General Public License version 3.
 */
#pragma once

#include "client_code.h"
#include "config.h"
#include "engine_actors.h"
#include "grass_doodads.h"
#include "wind_model.h"

#include <windows.h>
#include <d3dcompiler.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

struct GrassWindSettings
{
    bool wind;
    float amplitude;
    float flutter;
    float flutterSpeed;
    float anchor;
    float windStrength;
    bool modelBend;
    bool parting;
    float force;
    float radiusScale;
    float minRadius;
    float mountedScale;
};

GrassWindSettings GrassWindSettingsFrom(const Config& config);

constexpr int kGrassGridSide = 8;
constexpr float kGrassGridSpacing = 8.0f;
constexpr size_t kGrassGridRegisters = kGrassGridSide * kGrassGridSide;
constexpr size_t kGrassActorRegisters = kMaxNearbyUnits;
constexpr size_t kGrassUploadRegisters = kGrassGridRegisters + kGrassActorRegisters;
constexpr size_t kGrassConstantRegisters = 6;
constexpr float kGrassActorRange = 40.0f;
constexpr float kGrassFarAway = 1.0e5f;

struct GrassInputs
{
    float camera[3];
    float player[3];
    bool havePlayer;
    float dt;
    wind::Place place;
    const NearbyUnitFrame* units;
};

struct GrassFrame
{
    float registers[kGrassConstantRegisters][4];
    float upload[kGrassUploadRegisters][4];
};

const float* GrassGridRegisters(const GrassFrame& frame);
const float* GrassActorRegisters(const GrassFrame& frame);

namespace engine
{
constexpr uintptr_t kInitializeShaderConstants = 0x007B15D0;
constexpr uintptr_t kInitializeShaderConstantsSite = 0x007B2E25;
constexpr uintptr_t kSetupGrassShader = 0x007B10E0;
constexpr uintptr_t kSetupGrassShaderSite = 0x007985EE;
constexpr uintptr_t kDetailDoodadShaderConstants = 0x00D1C518;
constexpr size_t kDetailDoodadFirstFreeRegister = 14;
constexpr size_t kDetailDoodadUploadedRegisters = 23;
constexpr size_t kRegisterBytes = 4 * sizeof(float);
constexpr unsigned kGrassGridFirstRegister = 40;

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

class GrassMotion
{
public:
    void Configure(const GrassWindSettings& settings) { m_settings = settings; }
    const GrassWindSettings& Settings() const { return m_settings; }
    GrassShaderRewriter& Shaders() { return m_shaders; }
    void Build(const GrassInputs& in, wind::WindModel& model, GrassFrame& out);
    size_t TrackedCells() const { return m_cells.size(); }

private:
    struct Cell
    {
        float value[3] = {};
        float lastSeen = 0.0f;
        bool fresh = true;
    };

    void FillCells(const GrassInputs& in, wind::WindModel& model, float* grid);
    void ForgetOldCells();

    GrassWindSettings m_settings = GrassWindSettingsFrom(Config());
    GrassShaderRewriter m_shaders;
    std::unordered_map<uint64_t, Cell> m_cells;
    float m_clock = 0.0f;
};

GrassMotion& GlobalGrassWind();

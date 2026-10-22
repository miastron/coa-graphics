/*
 * Copyright (C) 2026 WarcraftXL
 * Derived from wxl-grasswind (https://github.com/WarcraftXL/wxl-grasswind), licensed under the GNU General Public
 * License version 3; see LICENSE.
 */
#include "grass_wind.h"

#include "log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace
{
constexpr float kTwoPi = 6.2831853f;
constexpr float kDegreesToRadians = kTwoPi / 360.0f;
constexpr float kMinWavelength = 0.1f;
constexpr float kMinCone = 0.01f;
constexpr float kMaxAnchor = 0.9f;
constexpr float kEpsilon = 1.0e-3f;
constexpr float kBladePhaseSpread = 1.7f;
constexpr float kCrossWaveSpeedScale = 1.37f;
constexpr float kPrimaryWaveCellX = 0.737f;
constexpr float kPrimaryWaveCellY = 1.311f;
constexpr uint32_t kWindClockWrapMs = 1000000;
constexpr float kMillisecondsToSeconds = 0.001f;
constexpr DWORD kVertexShader20 = 0xFFFE0200;
constexpr DWORD kVertexShader30 = 0xFFFE0300;
constexpr DWORD kCommentToken = 0xFFFE;
constexpr DWORD kEndToken = 0xFFFF;
constexpr DWORD kOpcodeMask = 0xFFFF;
constexpr DWORD kCommentLengthShift = 16;
constexpr DWORD kCommentLengthMask = 0x7FFF;
constexpr DWORD kInstructionLengthShift = 24;
constexpr DWORD kInstructionLengthMask = 0xF;
constexpr size_t kMaxShaderTokens = 4096;
constexpr size_t kVersionedKeyShift = 32;
constexpr const char* kShaderCompilerName = "d3dcompiler_47.dll";
constexpr const char* kAnchorInstruction = "addr0,r0,c3";
const char* const kGrassSignature[] = {"defc13,0,1,0,0", "dcl_normalv1", "dcl_colorv2", "dcl_texcoordv3",
                                       "addr0,r0,c3"};

constexpr size_t kStockVariants = 3;
constexpr uint32_t kStockLengths20[kStockVariants] = {552, 644, 788};
constexpr uint32_t kStockLengths30[kStockVariants] = {548, 676, 844};

const ClientCodeGuard kGrassWindCode[] = {
    {"detail doodad constants entry", engine::kInitializeShaderConstants, 16,
     {0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x5C, 0x01, 0x00, 0x00, 0x56, 0x6A, 0x00, 0x68, 0x70, 0x01, 0x00}},
    {"detail doodad constants block", 0x007B15DA, 12,
     {0x6A, 0x00, 0x68, 0x70, 0x01, 0x00, 0x00, 0x68, 0x18, 0xC5, 0xD1, 0x00}},
    {"detail doodad constants upload", 0x007B1194, 13,
     {0x6A, 0x17, 0x68, 0x18, 0xC5, 0xD1, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0xFF, 0xD0}},
};

const char* const kMotionBlock =
    "    dp3 r2.x, c0, r0\n"
    "    dp3 r2.y, c1, r0\n"
    "    add r2.xy, r2, c14\n"
    "    mov r4.xy, r2\n"
    "    dp3 r3.z, c2, r0\n"
    "    add r3.z, r3.z, c14.z\n"
    "    add r3.z, r3.z, -c16.z\n"
    "    add r3.z, r3.z, -c15.z\n"
    "    mul_sat r4.z, r3.z, c15.x\n"
    "    add r2.xy, r2, -c16\n"
    "    mul r2.z, r2.x, r2.x\n"
    "    mad r2.z, r2.y, r2.y, r2.z\n"
    "    add r2.z, r2.z, c15.w\n"
    "    rsq r2.w, r2.z\n"
    "    mul r3.x, r2.z, r2.w\n"
    "    mul_sat r3.y, r3.x, c15.y\n"
    "    mad r3.y, c17.x, r3.y, c17.y\n"
    "    slt r3.z, r3.x, c17.z\n"
    "    mul r3.y, r3.y, r3.z\n"
    "    mul r3.y, r3.y, r4.z\n"
    "    mul r3.y, r3.y, r2.w\n"
    "    mul r2.xy, r2, r3.y\n"
    "    add r5.z, c13.y, -v3.y\n"
    "    add r5.z, r5.z, c21.z\n"
    "    mul_sat r5.z, r5.z, c21.w\n"
    "    mul r5.z, r5.z, r5.z\n"
    "    mov r5.w, c13.y\n"
    "    mad r5.w, r0.z, c20.w, r5.w\n"
    "    rcp r5.w, r5.w\n"
    "    mul r5.z, r5.z, r5.w\n"
    "    mul r3.x, r4.x, c20.x\n"
    "    mad r3.x, r4.y, c20.y, r3.x\n"
    "    frc r3.x, r3.x\n"
    "    mad r2.z, r3.x, c21.y, c21.x\n"
    "    mul r5.z, r5.z, r2.z\n"
    "    mul r3.x, r3.x, c20.z\n"
    "    mul r3.y, r4.x, c18.z\n"
    "    mad r3.y, r4.y, c18.w, r3.y\n"
    "    add r3.y, r3.y, r3.x\n"
    "    add r3.y, r3.y, c17.w\n"
    "    mul r3.z, r4.x, c19.z\n"
    "    mad r3.z, r4.y, c19.w, r3.z\n"
    "    add r3.z, r3.z, r3.x\n"
    "    add r3.z, r3.z, c14.w\n"
    "    sincos r4.xy, r3.y\n"
    "    add r4.w, r4.y, c16.w\n"
    "    sincos r5.xy, r3.z\n"
    "    mul r3.zw, c18.xxxy, r4.wwww\n"
    "    mad r3.zw, c19.xxxy, r5.yyyy, r3\n"
    "    mul r3.zw, r3, r5.zzzz\n"
    "    add r2.xy, r2, r3.zwzw\n"
    "    mul r3.xyz, c0, r2.x\n"
    "    mad r3.xyz, c1, r2.y, r3.xyz\n"
    "    add r0.xyz, r0, r3\n";

bool StockLength(DWORD version, size_t bytes)
{
    if (version != kVertexShader20 && version != kVertexShader30)
        return false;
    const uint32_t* lengths = version == kVertexShader20 ? kStockLengths20 : kStockLengths30;
    for (size_t i = 0; i < kStockVariants; ++i)
        if (lengths[i] == bytes)
            return true;
    return false;
}

uint64_t CacheKey(DWORD version, size_t bytes)
{
    return (static_cast<uint64_t>(version) << kVersionedKeyShift) | bytes;
}

std::string WithoutSpaces(const std::string& line)
{
    std::string out;
    for (char c : line)
        if (c != ' ' && c != '\t' && c != '\r')
            out.push_back(c);
    return out;
}

bool LooksLikeGrass(const std::string& source)
{
    const std::string squeezed = WithoutSpaces(source);
    for (const char* needle : kGrassSignature)
        if (squeezed.find(needle) == std::string::npos)
            return false;
    return true;
}

size_t EndOfAnchorLine(const std::string& source)
{
    size_t lineStart = 0;
    while (lineStart < source.size())
    {
        size_t lineEnd = source.find('\n', lineStart);
        if (lineEnd == std::string::npos)
            lineEnd = source.size();
        if (WithoutSpaces(source.substr(lineStart, lineEnd - lineStart)) == kAnchorInstruction)
            return std::min(lineEnd + 1, source.size());
        lineStart = lineEnd + 1;
    }
    return std::string::npos;
}

std::string BlobText(ID3DBlob* blob)
{
    return std::string(static_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize());
}

float Clamped(float v, float lo, float hi)
{
    return std::min(std::max(v, lo), hi);
}
}

GrassWindSettings GrassWindSettingsFrom(const Config& c)
{
    return {c.grassWind,         c.grassWindDirection,     c.grassWindSpeed,         c.grassWindAmplitude,
            c.grassWindWavelength, c.grassWindCrossAmplitude, c.grassWindCrossWavelength, c.grassWindCrossAngle,
            c.grassWindLean,     c.grassWindVariance,      c.grassWindDistanceFade,  c.grassWindAnchor,
            c.grassParting,      c.grassPartingRadius,     c.grassPartingForce,      c.grassPartingEdgeForce,
            c.grassPartingHeight, c.grassPartingMinBladeHeight};
}

GrassConstants GrassFrameConstants(const GrassWindSettings& s, const float camera[3], const float* player,
                                   uint32_t tickMs)
{
    const float t = static_cast<float>(tickMs % kWindClockWrapMs) * kMillisecondsToSeconds;
    const float a1 = s.directionDeg * kDegreesToRadians;
    const float a2 = (s.directionDeg + s.crossAngleDeg) * kDegreesToRadians;
    const float k1 = kTwoPi / std::max(s.wavelength, kMinWavelength);
    const float k2 = kTwoPi / std::max(s.crossWavelength, kMinWavelength);
    const float d1[2] = {std::cos(a1), std::sin(a1)};
    const float d2[2] = {std::cos(a2), std::sin(a2)};
    const float amplitude1 = s.wind ? s.amplitude : 0.0f;
    const float amplitude2 = s.wind ? s.crossAmplitude : 0.0f;
    const float lean = s.wind ? s.lean : 0.0f;
    const float phase1 = -std::fmod(k1 * s.speed * t, kTwoPi);
    const float phase2 = -std::fmod(k2 * s.speed * kCrossWaveSpeedScale * t, kTwoPi);
    const bool parts = s.parting && player != nullptr;
    const float center = parts ? s.force : 0.0f;
    const float edge = parts ? s.edgeForce : 0.0f;
    const float coneHeight = std::max(s.coneHeight, kMinCone);
    const float radius = std::max(s.radius, kMinCone);
    const float variance = Clamped(s.variance, 0.0f, 1.0f);
    const float anchor = Clamped(s.anchor, 0.0f, kMaxAnchor);
    const float* subject = player ? player : camera;

    GrassConstants out = {{
        {camera[0], camera[1], camera[2], phase2},
        {1.0f / coneHeight, 1.0f / radius, s.minBladeHeight, kEpsilon},
        {subject[0], subject[1], subject[2], lean},
        {edge - center, center, radius, phase1},
        {d1[0] * amplitude1, d1[1] * amplitude1, d1[0] * k1, d1[1] * k1},
        {d2[0] * amplitude2, d2[1] * amplitude2, d2[0] * k2, d2[1] * k2},
        {kPrimaryWaveCellX, kPrimaryWaveCellY, kBladePhaseSpread, std::max(s.distanceFade, 0.0f)},
        {1.0f - variance * 0.5f, variance, -anchor, 1.0f / (1.0f - anchor)},
    }};
    return out;
}

namespace engine
{
const ClientCodeGuard* GrassWindGuards(size_t& count)
{
    count = sizeof(kGrassWindCode) / sizeof(kGrassWindCode[0]);
    return kGrassWindCode;
}

bool GrassWindCodeMatches(const ClientCodeView& code)
{
    for (const ClientCodeGuard& guard : kGrassWindCode)
        if (!ClientGuardMatches(code, guard))
        {
            VF_LOG_ERROR("grass wind: the client's %s at 0x%08X differs from the 12340 client; the grass stays still",
                         guard.name, static_cast<unsigned>(guard.address));
            return false;
        }
    return true;
}
}

ShaderCompiler LoadSystemShaderCompiler()
{
    HMODULE module = GetModuleHandleA(kShaderCompilerName);
    if (!module)
        module = LoadLibraryExA(kShaderCompilerName, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    ShaderCompiler compiler;
    if (!module)
        return compiler;
    compiler.disassemble = reinterpret_cast<ShaderDisassembleFn>(GetProcAddress(module, "D3DDisassemble"));
    compiler.assemble = reinterpret_cast<ShaderAssembleFn>(GetProcAddress(module, "D3DAssemble"));
    return compiler;
}

size_t ShaderTokenCount(const DWORD* code)
{
    if (!code)
        return 0;
    size_t index = 1;
    while (index < kMaxShaderTokens)
    {
        const DWORD token = code[index];
        const DWORD opcode = token & kOpcodeMask;
        if (opcode == kEndToken)
            return index + 1;
        if (opcode == kCommentToken)
            index += 1 + ((token >> kCommentLengthShift) & kCommentLengthMask);
        else
            index += 1 + ((token >> kInstructionLengthShift) & kInstructionLengthMask);
    }
    return 0;
}

void GrassShaderRewriter::UseCompiler(const ShaderCompiler& compiler)
{
    m_compiler = compiler;
    m_compilerLoaded = true;
}

bool GrassShaderRewriter::CompilerReady()
{
    if (!m_compilerLoaded)
    {
        m_compiler = LoadSystemShaderCompiler();
        m_compilerLoaded = true;
        if (!m_compiler.disassemble || !m_compiler.assemble)
            VF_LOG_ERROR("grass wind: %s or its D3DDisassemble and D3DAssemble are missing; the grass stays still",
                         kShaderCompilerName);
    }
    return m_compiler.disassemble && m_compiler.assemble;
}

std::vector<DWORD> GrassShaderRewriter::Build(const DWORD* code, size_t tokens, bool& foreign)
{
    const size_t bytes = tokens * sizeof(DWORD);
    ID3DBlob* text = nullptr;
    if (FAILED(m_compiler.disassemble(code, bytes, 0, nullptr, &text)) || !text)
    {
        VF_LOG_ERROR("grass wind: the stock grass vertex shader (%u bytes) could not be disassembled",
                     static_cast<unsigned>(bytes));
        return {};
    }
    std::string source = BlobText(text);
    text->Release();
    if (!LooksLikeGrass(source))
    {
        foreign = true;
        VF_LOG_INFO("grass wind: a vertex shader of a grass length (0x%08X, %u bytes) is not the grass shader; left "
                    "alone",
                    static_cast<unsigned>(code[0]), static_cast<unsigned>(bytes));
        return {};
    }
    size_t anchor = EndOfAnchorLine(source);
    if (source[anchor - 1] != '\n')
    {
        source.push_back('\n');
        anchor = source.size();
    }
    source.insert(anchor, kMotionBlock);
    ID3DBlob* assembled = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT result =
        m_compiler.assemble(source.c_str(), source.size(), "grassMotion", nullptr, nullptr, 0, &assembled, &errors);
    if (FAILED(result) || !assembled)
    {
        VF_LOG_ERROR("grass wind: the stock grass vertex shader (%u bytes) did not assemble with its motion: %s",
                     static_cast<unsigned>(bytes),
                     errors ? static_cast<const char*>(errors->GetBufferPointer()) : "no message");
        if (errors)
            errors->Release();
        if (assembled)
            assembled->Release();
        return {};
    }
    if (errors)
        errors->Release();
    std::vector<DWORD> out(assembled->GetBufferSize() / sizeof(DWORD));
    std::memcpy(out.data(), assembled->GetBufferPointer(), out.size() * sizeof(DWORD));
    assembled->Release();
    if (out.empty() || out[0] != code[0])
    {
        VF_LOG_ERROR("grass wind: the reassembled grass vertex shader (%u bytes) changed its shader model",
                     static_cast<unsigned>(bytes));
        return {};
    }
    VF_LOG_INFO("grass wind: motion injected into the stock grass vertex shader (0x%08X, %u -> %u bytes)",
                static_cast<unsigned>(code[0]), static_cast<unsigned>(bytes),
                static_cast<unsigned>(out.size() * sizeof(DWORD)));
    return out;
}

const DWORD* GrassShaderRewriter::Rewrite(const DWORD* code)
{
    if (!m_enabled || !code)
        return code;
    const size_t tokens = ShaderTokenCount(code);
    const size_t bytes = tokens * sizeof(DWORD);
    if (!tokens || !StockLength(code[0], bytes))
        return code;
    const uint64_t key = CacheKey(code[0], bytes);
    auto cached = m_cache.find(key);
    if (cached == m_cache.end())
    {
        std::vector<DWORD> rewritten;
        bool foreign = false;
        if (CompilerReady())
            rewritten = Build(code, tokens, foreign);
        (foreign ? m_foreign : rewritten.empty() ? m_failed : m_injected) += 1;
        cached = m_cache.emplace(key, std::move(rewritten)).first;
    }
    return cached->second.empty() ? code : cached->second.data();
}

GrassWind& GlobalGrassWind()
{
    static GrassWind grassWind;
    return grassWind;
}

/*
 * Copyright (C) 2026 WarcraftXL
 * Derived from wxl-grasswind (https://github.com/WarcraftXL/wxl-grasswind), licensed under the GNU General Public
 * License version 3; see LICENSE.
 *
 * Copyright (C) 2026 Morfium-G
 * The wind grid, the unit parting and the motion block derive from seyris-living-azeroth
 * (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU General Public License version 3.
 */
#include "grass_wind.h"

#include "log.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <string>

namespace
{
constexpr float kTwoPi = 6.2831853f;
constexpr float kMaxAnchor = 0.9f;
constexpr float kBladePhasePerYardX = 0.61f;
constexpr float kBladePhasePerYardY = 0.37f;
constexpr float kCellEaseSeconds = 0.35f;
constexpr float kCellForgetSeconds = 5.0f;
constexpr float kMaxFrameSeconds = 1.0f;
constexpr float kFallbackFrameSeconds = 1.0f / 60.0f;
constexpr size_t kCellsKeptPerGridCell = 4;
constexpr float kGridCellCentre = 0.5f;
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
constexpr uint64_t kContentHashBasis = 14695981039346656037ull;
constexpr uint64_t kContentHashPrime = 1099511628211ull;
constexpr const char* kShaderCompilerName = "d3dcompiler_47.dll";
constexpr const char* kAnchorInstruction = "addr0,r0,c3";
const char* const kVersionInstructions[] = {"vs_2_0", "vs_3_0"};
constexpr const char* kAlphaPassThrough = "movo1.w,v2.w";
const char* const kAlphaRestore = "    sge o1.w, v2.w, c39.y\n";
const char* const kGrassSignature[] = {"defc13,0,1,0,0", "dcl_normalv1", "dcl_colorv2", "dcl_texcoordv3",
                                       "addr0,r0,c3", kAlphaPassThrough};

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
    {"detail doodad shader setup entry", engine::kSetupGrassShader, 16,
     {0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x80, 0x00, 0x00, 0x00, 0xE8, 0x02, 0x2F, 0x0C, 0x00, 0x83, 0xF8}},
    {"detail doodad shader setup call", 0x007985EC, 10,
     {0x57, 0x51, 0xE8, 0xED, 0x8A, 0x01, 0x00, 0x83, 0xC4, 0x08}},
};

const char* const kMotionPrologue =
    "    def c35, 0.25, 4.0, 8.0, 0.0\n"
    "    def c36, 0.001, 3.0, 0.3, 8.0\n"
    "    def c37, 0.15915494, 0.5, 6.2831853, 3.1415927\n"
    "    def c38, 6.999, 2025.0, 0.0, 0.0\n"
    "    def c39, 255.0, 0.5, 0.125, 8.0\n"
    "    def c153, 0.015625, 0.6, 5.3, 0.4\n"
    "    def c154, 39.554175, 0.0, 2.0, 1.0\n"
    "    defi i0, 16, 0, 1, 0\n";

const char* const kMotionBlock =
    "    mad r16.x, v2.w, c39.x, c39.y\n"
    "    mul r16.x, r16.x, c153.x\n"
    "    frc r16.x, r16.x\n"
    "    mad r16.y, r16.x, c37.z, -c37.w\n"
    "    sincos r17.xy, r16.y\n"
    "    mul r16.z, r16.x, c153.z\n"
    "    frc r16.z, r16.z\n"
    "    mad r16.z, r16.z, c153.w, c153.y\n"
    "    mul r17.z, r17.x, c20.x\n"
    "    mad r17.z, -r17.y, c20.y, r17.z\n"
    "    mul r17.w, r17.y, c20.x\n"
    "    mad r17.w, r17.x, c20.y, r17.w\n"
    "    mul r17.zw, r17, r16.z\n"
    "    max r18.w, v1.z, c36.x\n"
    "    rcp r18.w, r18.w\n"
    "    mul r18.x, v1.x, r17.z\n"
    "    mad r18.x, v1.y, r17.w, r18.x\n"
    "    mul r18.z, r18.x, -r18.w\n"
    "    mov r18.xy, r17.zwzw\n"
    "    mul r19.xyz, c0, r18.x\n"
    "    mad r19.xyz, c1, r18.y, r19\n"
    "    mad r19.xyz, c2, r18.z, r19\n"
    "    add r0.xyz, r0, r19\n"
    "    dp3 r2.x, c0, r0\n"
    "    dp3 r2.y, c1, r0\n"
    "    dp3 r2.z, c2, r0\n"
    "    mul r23.x, r2.x, r2.x\n"
    "    mad r23.x, r2.y, r2.y, r23.x\n"
    "    add r23.x, r23.x, c36.x\n"
    "    rsq r23.y, r23.x\n"
    "    mul r23.x, r23.x, r23.y\n"
    "    mul r23.z, r16.x, c154.x\n"
    "    frc r23.z, r23.z\n"
    "    mul r23.z, r23.z, c21.x\n"
    "    mad r23.y, r23.z, -c20.w, c20.z\n"
    "    sge r23.z, r23.z, c154.w\n"
    "    add r23.y, r23.y, -r23.x\n"
    "    mad_sat r23.y, r23.y, -c21.y, c21.w\n"
    "    max r23.y, r23.y, r23.z\n"
    "    mul r23.y, r23.y, c154.z\n"
    "    mov r23.w, c20.w\n"
    "    slt r23.w, r23.w, c36.x\n"
    "    mad r23.x, r23.y, -r23.w, r23.y\n"
    "    mul r22.xyz, c1, v1.y\n"
    "    mad r22.xyz, v1.x, c0, r22\n"
    "    mad r22.xyz, v1.z, c2, r22\n"
    "    add r2.xyz, r2, c14\n"
    "    mad r10.x, v2.y, c39.x, c39.y\n"
    "    mul r10.x, r10.x, c39.z\n"
    "    frc r10.x, r10.x\n"
    "    mul r10.x, r10.x, c39.w\n"
    "    add r10.x, r10.x, -c39.y\n"
    "    mad r10.y, v2.x, c39.x, c39.y\n"
    "    mul r10.y, r10.y, c35.x\n"
    "    frc r10.y, r10.y\n"
    "    mul r10.y, r10.y, c35.y\n"
    "    add r10.y, r10.y, -c39.y\n"
    "    mad r10.x, r10.y, c35.z, r10.x\n"
    "    mad r10.x, r10.x, c19.x, c19.y\n"
    "    mova a0.x, r10.x\n"
    "    mov r11, c120[a0.x]\n"
    "    add r3.w, v3.y, -r11.x\n"
    "    mul_sat r3.w, r3.w, r11.y\n"
    "    add r3.w, r3.w, -c15.x\n"
    "    mul_sat r3.w, r3.w, c15.y\n"
    "    mul r3.w, r3.w, r3.w\n"
    "    mul r4.xy, r2, c16\n"
    "    add r4.xy, r4, -c16.zw\n"
    "    max r4.xy, r4, c13.x\n"
    "    min r4.xy, r4, c38.x\n"
    "    frc r5.xy, r4\n"
    "    add r4.xy, r4, -r5\n"
    "    mad r4.x, r4.y, c36.w, r4.x\n"
    "    mova a0.x, r4.x\n"
    "    mov r6, c40[a0.x]\n"
    "    mov r7, c41[a0.x]\n"
    "    lrp r8, r5.x, r7, r6\n"
    "    mov r6, c48[a0.x]\n"
    "    mov r7, c49[a0.x]\n"
    "    lrp r9, r5.x, r7, r6\n"
    "    lrp r6, r5.y, r9, r8\n"
    "    mov r7.x, c14.w\n"
    "    mad r7.x, r2.x, c17.y, r7.x\n"
    "    mad r7.x, r2.y, c17.z, r7.x\n"
    "    mad r7.x, r7.x, c37.x, c37.y\n"
    "    frc r7.x, r7.x\n"
    "    mad r7.x, r7.x, c37.z, -c37.w\n"
    "    sincos r8.xy, r7.x\n"
    "    add r7.y, r6.z, c36.z\n"
    "    mul r7.y, r7.y, c15.w\n"
    "    mad r7.z, r7.y, r8.y, c15.z\n"
    "    mul r8.xy, r6, r7.z\n"
    "    mul r8.xy, r8, r11.z\n"
    "    mov r14, c13.x\n"
    "    add r15.xy, r2, -c18\n"
    "    mul r15.z, r15.x, r15.x\n"
    "    mad r15.z, r15.y, r15.y, r15.z\n"
    "    if_lt r15.z, c38.y\n"
    "    loop aL, i0\n"
    "    add r9.xy, r2, -c104[aL]\n"
    "    mul r9.z, r9.x, r9.x\n"
    "    mad r9.z, r9.y, r9.y, r9.z\n"
    "    add r9.z, r9.z, c36.x\n"
    "    rsq r9.w, r9.z\n"
    "    mul r9.z, r9.z, r9.w\n"
    "    rcp r7.w, c104[aL].w\n"
    "    mul r9.z, r9.z, r7.w\n"
    "    add r9.z, c13.y, -r9.z\n"
    "    max r9.z, r9.z, c13.x\n"
    "    add r7.w, r2.z, -c104[aL].z\n"
    "    abs r7.w, r7.w\n"
    "    add r7.w, c36.y, -r7.w\n"
    "    max r7.w, r7.w, c13.x\n"
    "    min r7.w, r7.w, c13.y\n"
    "    mul r9.z, r9.z, r7.w\n"
    "    mul r9.z, r9.z, r9.w\n"
    "    mad r14.xy, r9, r9.z, r14\n"
    "    endloop\n"
    "    endif\n"
    "    mul r14.xy, r14, c17.w\n"
    "    mul r14.xy, r14, r11.w\n"
    "    add r8.xy, r8, r14\n"
    "    mul r8.xy, r8, r3.w\n"
    "    mul r4.xyz, c0, r8.x\n"
    "    mad r4.xyz, c1, r8.y, r4\n"
    "    add r0.xyz, r0, r4\n"
    "    mad r0.xyz, r22, -r23.x, r0\n";

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

uint64_t ContentKey(const DWORD* code, size_t tokens)
{
    uint64_t hash = kContentHashBasis;
    for (size_t i = 0; i < tokens; ++i)
        for (size_t shift = 0; shift < sizeof(DWORD) * CHAR_BIT; shift += CHAR_BIT)
            hash = (hash ^ ((code[i] >> shift) & 0xFF)) * kContentHashPrime;
    return hash;
}

std::string WithoutSpaces(const std::string& line)
{
    std::string out;
    for (char c : line)
        if (c != ' ' && c != '\t' && c != '\r')
            out.push_back(c);
    return out;
}

std::string MissingSignature(const std::string& source)
{
    const std::string squeezed = WithoutSpaces(source);
    std::string missing;
    for (const char* needle : kGrassSignature)
        if (squeezed.find(needle) == std::string::npos)
            missing += (missing.empty() ? "" : ", ") + std::string(needle);
    return missing;
}

template <typename Match>
size_t EndOfLineWhere(const std::string& source, Match matches)
{
    size_t lineStart = 0;
    while (lineStart < source.size())
    {
        size_t lineEnd = source.find('\n', lineStart);
        if (lineEnd == std::string::npos)
            lineEnd = source.size();
        if (matches(WithoutSpaces(source.substr(lineStart, lineEnd - lineStart))))
            return std::min(lineEnd + 1, source.size());
        lineStart = lineEnd + 1;
    }
    return std::string::npos;
}

size_t EndOfAnchorLine(const std::string& source)
{
    return EndOfLineWhere(source, [](const std::string& line) { return line == kAnchorInstruction; });
}

bool ReplaceLineWhere(std::string& source, const char* squeezed, const char* replacement)
{
    const size_t end = EndOfLineWhere(source, [squeezed](const std::string& line) { return line == squeezed; });
    if (end == std::string::npos)
        return false;
    const size_t lineEnd = source[end - 1] == '\n' ? end - 1 : end;
    const size_t previous = lineEnd ? source.rfind('\n', lineEnd - 1) : std::string::npos;
    const size_t from = previous == std::string::npos ? 0 : previous + 1;
    source.replace(from, end - from, replacement);
    return true;
}

size_t EndOfVersionLine(const std::string& source)
{
    return EndOfLineWhere(source, [](const std::string& line) {
        for (const char* version : kVersionInstructions)
            if (line == version)
                return true;
        return false;
    });
}

std::string BlobText(ID3DBlob* blob)
{
    return std::string(static_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize());
}

float Clamped(float v, float lo, float hi)
{
    return std::min(std::max(v, lo), hi);
}

uint64_t CellKey(int x, int y)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
}

void ClearActors(float (*actors)[4])
{
    for (size_t i = 0; i < kGrassActorRegisters; ++i)
    {
        actors[i][0] = actors[i][1] = actors[i][2] = kGrassFarAway;
        actors[i][3] = 0.0f;
    }
}
}

GrassWindSettings GrassWindSettingsFrom(const Config& c)
{
    return {c.grassWind,
            c.grassWindAmplitude,
            c.grassWindFlutter,
            c.grassWindFlutterSpeed,
            c.grassWindAnchor,
            c.windStrength,
            c.grassModelBend,
            c.grassParting,
            c.grassPartingForce,
            c.grassPartingRadiusScale,
            c.grassPartingMinRadius,
            c.grassPartingMountedScale};
}

const float* GrassGridRegisters(const GrassFrame& frame)
{
    return &frame.upload[0][0];
}

const float* GrassActorRegisters(const GrassFrame& frame)
{
    return &frame.upload[kGrassGridRegisters][0];
}

void GrassMotion::FillCells(const GrassInputs& in, wind::WindModel& model, float* grid)
{
    const float* subject = in.havePlayer ? in.player : in.camera;
    const int baseX = static_cast<int>(std::floor(subject[0] / kGrassGridSpacing)) - kGrassGridSide / 2;
    const int baseY = static_cast<int>(std::floor(subject[1] / kGrassGridSpacing)) - kGrassGridSide / 2;
    const float ease = 1.0f - std::exp(-in.dt / kCellEaseSeconds);
    for (int iy = 0; iy < kGrassGridSide; ++iy)
        for (int ix = 0; ix < kGrassGridSide; ++ix)
        {
            const int cx = baseX + ix;
            const int cy = baseY + iy;
            Cell& cell = m_cells[CellKey(cx, cy)];
            cell.lastSeen = m_clock;
            float target[3] = {};
            if (m_settings.wind)
            {
                const float position[3] = {(cx + kGridCellCentre) * kGrassGridSpacing,
                                           (cy + kGridCellCentre) * kGrassGridSpacing, subject[2]};
                const wind::Sample sample = model.At(position);
                target[0] = sample.dirX * sample.strength;
                target[1] = sample.dirY * sample.strength;
                target[2] = sample.gust;
            }
            for (int k = 0; k < 3; ++k)
                cell.value[k] = cell.fresh ? target[k] : cell.value[k] + (target[k] - cell.value[k]) * ease;
            cell.fresh = false;
            float* out = grid + (iy * kGrassGridSide + ix) * 4;
            out[0] = cell.value[0];
            out[1] = cell.value[1];
            out[2] = cell.value[2];
            out[3] = 0.0f;
        }
}

void GrassMotion::ForgetOldCells()
{
    if (m_cells.size() <= kGrassGridRegisters * kCellsKeptPerGridCell)
        return;
    for (auto it = m_cells.begin(); it != m_cells.end();)
        it = (m_clock - it->second.lastSeen > kCellForgetSeconds || m_clock < it->second.lastSeen)
                 ? m_cells.erase(it)
                 : std::next(it);
}

void GrassMotion::Build(const GrassInputs& in, wind::WindModel& model, GrassFrame& out)
{
    const GrassWindSettings& s = m_settings;
    const float dt = in.dt >= 0.0f && in.dt <= kMaxFrameSeconds ? in.dt : kFallbackFrameSeconds;
    GrassInputs timed = in;
    timed.dt = dt;
    m_clock += dt;
    model.SetStrength(s.windStrength);
    model.Update(dt, in.place);

    const float* subject = in.havePlayer ? in.player : in.camera;
    const float anchor = Clamped(s.anchor, 0.0f, kMaxAnchor);
    const float inverseSpacing = 1.0f / kGrassGridSpacing;
    const int baseX = static_cast<int>(std::floor(subject[0] * inverseSpacing)) - kGrassGridSide / 2;
    const int baseY = static_cast<int>(std::floor(subject[1] * inverseSpacing)) - kGrassGridSide / 2;
    const float phase = std::fmod(m_clock * s.flutterSpeed, kTwoPi);
    const float amplitude = s.wind ? s.amplitude : 0.0f;
    const float flutter = s.wind ? s.flutter : 0.0f;
    const float push = s.parting ? s.force : 0.0f;
    const float registers[kGrassConstantRegisters][4] = {
        {in.camera[0], in.camera[1], in.camera[2], phase},
        {anchor, 1.0f / (1.0f - anchor), amplitude, flutter},
        {inverseSpacing, inverseSpacing, baseX + kGridCellCentre, baseY + kGridCellCentre},
        {0.0f, kBladePhasePerYardX, kBladePhasePerYardY, push},
        {subject[0], subject[1], subject[2], 0.0f},
        {s.modelBend ? 1.0f : 0.0f, s.modelBend ? 0.0f : static_cast<float>(kGrassFallbackEntry), 0.0f, 0.0f},
    };
    std::memcpy(out.registers, registers, sizeof(registers));

    FillCells(timed, model, &out.upload[0][0]);
    ForgetOldCells();

    float(*actors)[4] = &out.upload[kGrassGridRegisters];
    ClearActors(actors);
    if (!s.parting || !in.units)
        return;
    for (uint32_t i = 0; i < in.units->count && i < kGrassActorRegisters; ++i)
    {
        const NearbyUnit& unit = in.units->units[i];
        const float radius = std::max(unit.radius * s.radiusScale * (unit.mounted ? s.mountedScale : 1.0f),
                                      s.minRadius);
        actors[i][0] = unit.position[0];
        actors[i][1] = unit.position[1];
        actors[i][2] = unit.position[2];
        actors[i][3] = radius;
    }
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
    const std::string missing = MissingSignature(source);
    if (!missing.empty())
    {
        foreign = true;
        VF_LOG_INFO("grass wind: a vertex shader of a grass length (0x%08X, %u bytes) is not the grass shader (no %s); "
                    "left alone",
                    static_cast<unsigned>(code[0]), static_cast<unsigned>(bytes), missing.c_str());
        return {};
    }
    size_t anchor = EndOfAnchorLine(source);
    if (source[anchor - 1] != '\n')
    {
        source.push_back('\n');
        anchor = source.size();
    }
    source.insert(anchor, kMotionBlock);
    if (!ReplaceLineWhere(source, kAlphaPassThrough, kAlphaRestore))
    {
        VF_LOG_ERROR("grass wind: the stock grass vertex shader (%u bytes) has no alpha pass-through to restore",
                     static_cast<unsigned>(bytes));
        return {};
    }
    const size_t version = EndOfVersionLine(source);
    if (version == std::string::npos)
    {
        VF_LOG_ERROR("grass wind: the stock grass vertex shader (%u bytes) has no version line to define the motion "
                     "constants after",
                     static_cast<unsigned>(bytes));
        return {};
    }
    source.insert(version, kMotionPrologue);
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
    const uint64_t key = ContentKey(code, tokens);
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

GrassMotion& GlobalGrassWind()
{
    static GrassMotion grassWind;
    return grassWind;
}

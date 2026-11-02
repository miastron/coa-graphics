/*
 * Copyright (C) 2026 Morfium-G
 * The terrain shader rewrite (world position handed to the pixel shader, the stock fog ending as the anchor) derives
 * from seyris-living-azeroth (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU General
 * Public License version 3.
 */
#include "snow_shaders.h"

#include "log.h"

#include <cctype>
#include <cstring>
#include <regex>

namespace
{
constexpr DWORD kVertexShader30 = 0xFFFE0300;
constexpr DWORD kPixelShader30 = 0xFFFF0300;
constexpr size_t kMaxVertexOutputs = 11;
constexpr uint64_t kContentHashBasis = 14695981039346656037ull;
constexpr uint64_t kContentHashPrime = 1099511628211ull;
constexpr const char* kLiftedPosition = "r12";
constexpr const char* kPositionInput = "v0";

const char* const kVertexPrologue = "    def c203, 0, 1, 0, 0";

const char* const kLiftBlock =
    "    mul r9.xy, v0.xy, c200.x\n"
    "    mov r9.zw, c203.x\n"
    "    texldl r10, r9, s0\n"
    "    add r11.xy, v0.xy, -c201.xy\n"
    "    add r11.zw, -v0.xyxy, c201.zwzw\n"
    "    min r11.xy, r11.xy, r11.zw\n"
    "    min r11.x, r11.x, r11.y\n"
    "    mul_sat r11.x, r11.x, c202.x\n"
    "    mul r11.x, r11.x, c202.y\n"
    "    mul r11.x, r11.x, r10.x\n"
    "    mov r12, v0\n"
    "    mad r12.z, r11.x, c200.z, r12.z\n";

const char* const kViewDepthBlock =
    "    mul r9.x, r12.y, c1.z\n"
    "    mad r9.x, r12.x, c0.z, r9.x\n"
    "    mad r9.x, r12.z, c2.z, r9.x\n"
    "    add $.w, r9.x, c3.z\n";

const char* const kPixelPrologue =
    "    def c216, 0.5, 1, 0, 0.0001\n"
    "    def c217, 0.2126, 0.7152, 0.0722, 0\n"
    "    def c208, 0.25, 0.25, 0.0875, 0.0833333358\n"
    "    def c209, 2, 0.00390625, -1000, 0\n"
    "    dcl_texcoord7 v9\n"
    "    dcl_2d s9\n"
    "    dcl_2d s10\n";

const char* const kWhitenBlock =
    "    mul r20.xy, v9.xy, c220.x\n"
    "    texld r21, r20, s9\n"
    "    mul r20.xy, r20.xy, c220.y\n"
    "    texld r22, r20, s9\n"
    "    add r23.x, r21.z, r22.z\n"
    "    add r23.x, r23.x, -c216.y\n"
    "    mad r23.x, r23.x, c220.z, r21.y\n"
    "    mad_sat r23.x, r23.x, c223.x, c223.y\n"
    "    add_sat r23.x, r23.x, -r21.y\n"
    "    mul r23.x, r23.x, r21.w\n"
    "    add r24.xy, v9.xy, -c221.xy\n"
    "    add r24.zw, -v9.xyxy, c221.zwzw\n"
    "    min r24.xy, r24.xy, r24.zw\n"
    "    min r24.x, r24.x, r24.y\n"
    "    mul_sat r24.x, r24.x, c222.x\n"
    "    mul r23.x, r23.x, r24.x\n"
    "    mul r23.x, r23.x, c220.w\n"
    "    mul r25.xyz, v0.xyz, c222.z\n"
    "    dp3 r26.x, $, c217\n"
    "    dp3 r26.y, r25, c217\n"
    "    max r26.y, r26.y, c216.w\n"
    "    rcp r26.y, r26.y\n"
    "    mul r26.x, r26.x, r26.y\n"
    "    mul_sat r26.x, r26.x, c222.w\n"
    "    mul r25.xyz, r25, r26.x\n"
    "    lrp r27.xyz, r23.x, r25, $\n"
    "    mov $.xyz, r27\n";

const char* const kDepthOpen = "    mov r27.w, v9.w\n";

const char* const kTrenchOpen =
    "    add r11.xy, v9.xy, -c205.xy\n"
    "    add r11.zw, -v9.xyxy, c205.zwzw\n"
    "    min r11.xy, r11.xy, r11.zw\n"
    "    min r11.x, r11.x, r11.y\n"
    "    mul r11.y, r21.x, c204.w\n"
    "    min r11.x, r11.x, r11.y\n"
    "    if_gt r11.x, c216.z\n"
    "    mul r12.w, r21.x, c206.w\n"
    "    add r12.xyz, v9.xyz, -c206.xyz\n"
    "    nrm r16.xyz, r12\n"
    "    max r16.w, -r16.z, c208.y\n"
    "    rcp r16.w, r16.w\n"
    "    mul r16.w, r16.w, r12.w\n"
    "    mul r16.w, r16.w, c208.w\n"
    "    mul r15.xy, r16.xy, r16.w\n"
    "    mul r15.z, r12.w, -c208.w\n"
    "    mov r13.xy, v9.xy\n"
    "    mov r13.z, c216.z\n"
    "    mov r14.xy, v9.xy\n"
    "    mov r14.zw, c216.z\n"
    "    mov r11.z, c209.z\n"
    "    mov r11.w, c216.z\n";

const char* const kTrenchProbe =
    "    mul r16.xy, r13.xy, c204.x\n"
    "    mov r16.zw, c216.z\n"
    "    texldl r17, r16, s10\n"
    "    mad r17.z, r17.y, c204.z, -r17.x\n"
    "    mul r17.z, r17.z, r12.w\n"
    "    add r17.z, r17.z, -r13.z\n"
    "    cmp r17.w, r17.z, c216.y, c216.z\n"
    "    add r18.w, c216.y, -r14.w\n"
    "    mul r17.w, r17.w, r18.w\n"
    "    lrp r18.xyz, r17.w, r13, r14\n"
    "    mov r14.xyz, r18\n"
    "    lrp r19.x, r17.w, r17.z, r11.w\n"
    "    mov r11.w, r19.x\n"
    "    max r14.w, r14.w, r17.w\n"
    "    add r18.w, c216.y, -r14.w\n"
    "    lrp r19.y, r18.w, r17.z, r11.z\n"
    "    mov r11.z, r19.y\n";

const char* const kTrenchStep = "    add r13.xyz, r13, r15\n";

const char* const kTrenchRefine =
    "    add r16.x, r11.w, -r11.z\n"
    "    rcp r16.x, r16.x\n"
    "    mul r16.x, r16.x, r11.w\n"
    "    mad r14.xyz, r15, -r16.x, r14\n";

const char* const kTrenchDepth =
    "    add r16.xy, r14.xy, -v9.xy\n"
    "    mov r16.z, r14.z\n"
    "    dp3 r16.x, r16, r16\n"
    "    dp3 r16.y, r12, r12\n"
    "    rcp r16.y, r16.y\n"
    "    mul r16.x, r16.x, r16.y\n"
    "    rsq r16.x, r16.x\n"
    "    rcp r16.x, r16.x\n"
    "    mad r27.w, r27.w, r16.x, r27.w\n";

const char* const kTrenchShade =
    "    mul r16.xy, r14.xy, c204.x\n"
    "    mov r16.zw, c216.z\n"
    "    mov r17, r16\n"
    "    add r17.x, r16.x, -c209.y\n"
    "    texldl r17, r17, s10\n"
    "    mov r18, r16\n"
    "    add r18.x, r16.x, c209.y\n"
    "    texldl r18, r18, s10\n"
    "    mov r19, r16\n"
    "    add r19.y, r16.y, -c209.y\n"
    "    texldl r19, r19, s10\n"
    "    mov r20, r16\n"
    "    add r20.y, r16.y, c209.y\n"
    "    texldl r20, r20, s10\n"
    "    mad r10.x, r17.y, c204.z, -r17.x\n"
    "    mad r10.y, r18.y, c204.z, -r18.x\n"
    "    mad r10.z, r19.y, c204.z, -r19.x\n"
    "    mad r10.w, r20.y, c204.z, -r20.x\n"
    "    mul r10, r10, r12.w\n"
    "    add r11.x, r10.x, -r10.y\n"
    "    add r11.y, r10.z, -r10.w\n"
    "    mov r11.z, c216.x\n"
    "    nrm r12.xyz, r11\n"
    "    add r17.x, r17.x, r18.x\n"
    "    add r17.x, r17.x, r19.x\n"
    "    add r17.x, r17.x, r20.x\n"
    "    dp3_sat r11.x, r12, c207\n"
    "    mov_sat r11.y, c207.z\n"
    "    mov r11.z, -c207.w\n"
    "    add r11.z, r11.z, c216.y\n"
    "    mad r11.x, r11.x, r11.z, c207.w\n"
    "    mad r11.y, r11.y, r11.z, c207.w\n"
    "    max r11.y, r11.y, c216.w\n"
    "    rcp r11.y, r11.y\n"
    "    mul r11.x, r11.x, r11.y\n"
    "    min r11.x, r11.x, c209.x\n"
    "    mul r11.y, r17.x, -c208.z\n"
    "    add r11.y, r11.y, c216.y\n"
    "    mul r11.x, r11.x, r11.y\n"
    "    mul $.xyz, $, r11.x\n"
    "    endif\n";

const char* const kDepthWrite =
    "    mul r27.w, r27.w, c210.z\n"
    "    rcp r27.w, r27.w\n"
    "    mad oDepth, r27.w, c210.y, c210.x\n";

constexpr int kTrenchSteps = 12;

const std::regex kVertexReserved(R"(\b(c20[0-3]|r9|r1[0-2]|s0)\b|dcl_texcoord7|dcl_2d)");
const std::regex kViewTransform(R"(\bmad r0, v0\.x, c0, r0\b)");
const std::regex kPixelReserved(
    R"(\b(v9|s9|s10|c20[4-9]|c21[0-9]|c22[0-3]|r1[0-9]|r2[0-7]|oDepth)\b|dcl_texcoord7)");
const std::regex kOutputDeclaration(R"(^\s*dcl_\w+ o(\d+))");
const std::regex kViewOutput(R"(^\s*dcl_texcoord3 (o\d+)\.xyz\s*$)");
const std::regex kFogEnding(R"(\n[ \t]*add (r\d+)\.xyz, \1, -c2[ \t]*\n[ \t]*mad oC0\.xyz, v\d+\.x, \1, c2)");

std::vector<std::string> Lines(const std::string& source)
{
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= source.size())
    {
        size_t end = source.find('\n', start);
        if (end == std::string::npos)
            end = source.size();
        std::string line = source.substr(start, end - start);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
        start = end + 1;
    }
    return lines;
}

std::string Joined(const std::vector<std::string>& lines)
{
    std::string out;
    for (const std::string& line : lines)
        out += line + "\n";
    return out;
}

std::string Trimmed(const std::string& line)
{
    const size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos)
        return "";
    const size_t last = line.find_last_not_of(" \t");
    return line.substr(first, last - first + 1);
}

bool Header(const std::string& trimmed)
{
    return trimmed.empty() || trimmed.rfind("dcl", 0) == 0 || trimmed.rfind("def", 0) == 0 ||
           trimmed.rfind("//", 0) == 0;
}

size_t FirstInstruction(const std::vector<std::string>& lines)
{
    for (size_t i = 1; i < lines.size(); ++i)
        if (!Header(Trimmed(lines[i])))
            return i;
    return lines.size();
}

bool RegisterBoundary(char c)
{
    return !std::isalnum(static_cast<unsigned char>(c)) && c != '_';
}

std::string Renamed(const std::string& line, const char* from, const char* to)
{
    const size_t length = std::strlen(from);
    std::string out;
    size_t i = 0;
    while (i < line.size())
    {
        const bool starts = line.compare(i, length, from) == 0 && (i == 0 || RegisterBoundary(line[i - 1])) &&
                            (i + length == line.size() || !std::isdigit(static_cast<unsigned char>(line[i + length])));
        if (starts)
        {
            out += to;
            i += length;
        }
        else
        {
            out += line[i++];
        }
    }
    return out;
}

std::vector<std::string> BlockLines(const char* block)
{
    std::vector<std::string> lines = Lines(block);
    if (!lines.empty() && lines.back().empty())
        lines.pop_back();
    return lines;
}

uint64_t ContentKey(const DWORD* code, size_t tokens, SnowShaderKind kind)
{
    uint64_t hash = kContentHashBasis ^ static_cast<uint64_t>(kind);
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(code);
    for (size_t i = 0; i < tokens * sizeof(DWORD); ++i)
    {
        hash ^= bytes[i];
        hash *= kContentHashPrime;
    }
    return hash;
}

std::string BlobText(ID3DBlob* blob)
{
    const char* text = static_cast<const char*>(blob->GetBufferPointer());
    return std::string(text, strnlen(text, blob->GetBufferSize()));
}

const char* KindName(SnowShaderKind kind)
{
    return kind == SnowShaderKind::Vertex ? "vertex" : "pixel";
}
}

bool EditTerrainVertexSource(std::string& source, std::string& why)
{
    std::vector<std::string> lines = Lines(source);
    if (lines.empty() || Trimmed(lines[0]) != "vs_3_0")
    {
        why = "not vs_3_0";
        return false;
    }
    if (source.find("dcl_position v0") == std::string::npos)
    {
        why = "no position input v0";
        return false;
    }
    if (std::regex_search(source, kVertexReserved))
    {
        why = "a register the lift needs is in use";
        return false;
    }
    if (!std::regex_search(source, kViewTransform))
    {
        why = "no view transform in c0 to c3";
        return false;
    }
    const size_t first = FirstInstruction(lines);
    std::vector<std::string> head(lines.begin(), lines.begin() + first);
    std::vector<std::string> body;
    for (size_t i = first; i < lines.size(); ++i)
        body.push_back(Renamed(lines[i], kPositionInput, kLiftedPosition));
    int highest = -1;
    size_t outputs = 0;
    for (const std::string& line : head)
    {
        std::smatch m;
        if (std::regex_search(line, m, kOutputDeclaration))
        {
            ++outputs;
            highest = std::max(highest, std::stoi(m[1].str()));
        }
    }
    std::vector<std::string> worldOutput;
    std::string output;
    if (outputs < kMaxVertexOutputs)
    {
        output = "o" + std::to_string(highest + 1);
        head.push_back("    dcl_texcoord7 " + output);
        worldOutput.push_back("    mov " + output + ".xyz, " + kLiftedPosition);
    }
    else
    {
        size_t viewDeclaration = head.size();
        for (size_t i = 0; i < head.size(); ++i)
        {
            std::smatch m;
            if (std::regex_search(head[i], m, kViewOutput))
            {
                viewDeclaration = i;
                output = m[1].str();
            }
        }
        if (viewDeclaration == head.size())
        {
            why = "all outputs in use and no view position output to take over";
            return false;
        }
        const std::string viewWrite = "mov " + output + ".xyz, r0";
        size_t writes = 0;
        for (std::string& line : body)
            if (Trimmed(line) == viewWrite)
            {
                line = "    mov " + output + ".xyz, " + kLiftedPosition;
                ++writes;
            }
        if (writes != 1)
        {
            why = "the view position output is not written once from r0";
            return false;
        }
        head[viewDeclaration] = "    dcl_texcoord7 " + output;
    }
    head.insert(head.begin() + 1, kVertexPrologue);
    head.push_back("    dcl_2d s0");
    std::vector<std::string> out = head;
    for (const std::string& line : BlockLines(kLiftBlock))
        out.push_back(line);
    for (const std::string& line : BlockLines(kViewDepthBlock))
        out.push_back(Renamed(line, "$", output.c_str()));
    out.insert(out.end(), worldOutput.begin(), worldOutput.end());
    out.insert(out.end(), body.begin(), body.end());
    source = Joined(out);
    return true;
}

bool EditTerrainPixelSource(std::string& source, std::string& why, bool writeDepth)
{
    std::vector<std::string> lines = Lines(source);
    if (lines.empty() || Trimmed(lines[0]) != "ps_3_0")
    {
        why = "not ps_3_0";
        return false;
    }
    if (source.find("dcl_color v0") == std::string::npos)
    {
        why = "no light colour input v0";
        return false;
    }
    if (std::regex_search(source, kPixelReserved))
    {
        why = "a register the snow edges need is in use";
        return false;
    }
    std::smatch m;
    if (!std::regex_search(source, m, kFogEnding))
    {
        why = "no stock fog ending";
        return false;
    }
    std::string block = std::string(kWhitenBlock) + (writeDepth ? kDepthOpen : "") + kTrenchOpen + kTrenchProbe;
    for (int step = 0; step < kTrenchSteps; ++step)
        block += std::string(kTrenchStep) + kTrenchProbe;
    block += std::string(kTrenchRefine) + (writeDepth ? kTrenchDepth : "") + kTrenchShade +
             (writeDepth ? kDepthWrite : "");
    const std::string colour = m[1].str();
    for (size_t at = block.find('$'); at != std::string::npos; at = block.find('$', at + colour.size()))
        block.replace(at, 1, colour);
    const size_t ending = static_cast<size_t>(m.position(0));
    source.insert(ending + 1, block);
    source.insert(source.find('\n') + 1, kPixelPrologue);
    return true;
}

void SnowShaderRewriter::UseCompiler(const ShaderCompiler& compiler)
{
    m_compiler = compiler;
    m_compilerLoaded = true;
}

bool SnowShaderRewriter::CompilerReady()
{
    if (!m_compilerLoaded)
    {
        m_compiler = LoadSystemShaderCompiler();
        m_compilerLoaded = true;
        if (!m_compiler.disassemble || !m_compiler.assemble)
            VF_LOG_ERROR("snow: D3DDisassemble and D3DAssemble are missing; the terrain stays bare");
    }
    return m_compiler.disassemble && m_compiler.assemble;
}

std::vector<DWORD> SnowShaderRewriter::Build(const DWORD* code, size_t tokens, SnowShaderKind kind, bool& left)
{
    const size_t bytes = tokens * sizeof(DWORD);
    ID3DBlob* text = nullptr;
    if (FAILED(m_compiler.disassemble(code, bytes, 0, nullptr, &text)) || !text)
    {
        VF_LOG_ERROR("snow: a terrain %s shader (%u bytes) could not be disassembled", KindName(kind),
                     static_cast<unsigned>(bytes));
        return {};
    }
    std::string source = BlobText(text);
    text->Release();
    std::string why;
    const bool edited = kind == SnowShaderKind::Vertex
                            ? EditTerrainVertexSource(source, why)
                            : EditTerrainPixelSource(source, why, kind == SnowShaderKind::DepthPixel);
    if (!edited)
    {
        left = kind != SnowShaderKind::Vertex;
        if (kind == SnowShaderKind::Pixel)
            VF_LOG_INFO("snow: a terrain pixel shader (%u bytes) is left stock (%s)", static_cast<unsigned>(bytes),
                        why.c_str());
        else
            VF_LOG_ERROR("snow: a terrain vertex shader (%u bytes) cannot carry the snow (%s); the snow stays off",
                         static_cast<unsigned>(bytes), why.c_str());
        return {};
    }
    ID3DBlob* assembled = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT result =
        m_compiler.assemble(source.c_str(), source.size(), "terrainSnow", nullptr, nullptr, 0, &assembled, &errors);
    if (FAILED(result) || !assembled)
    {
        VF_LOG_ERROR("snow: a terrain %s shader (%u bytes) did not assemble with the snow: %s", KindName(kind),
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
        VF_LOG_ERROR("snow: the reassembled terrain %s shader (%u bytes) changed its shader model", KindName(kind),
                     static_cast<unsigned>(bytes));
        return {};
    }
    return out;
}

const DWORD* SnowShaderRewriter::Rewrite(const DWORD* code, SnowShaderKind kind)
{
    if (!m_enabled || !code)
        return code;
    const DWORD version = kind == SnowShaderKind::Vertex ? kVertexShader30 : kPixelShader30;
    const size_t tokens = ShaderTokenCount(code);
    if (!tokens || code[0] != version)
    {
        if (kind == SnowShaderKind::Vertex)
            ++m_vertexFailed;
        return code;
    }
    const uint64_t key = ContentKey(code, tokens, kind);
    auto cached = m_cache.find(key);
    if (cached == m_cache.end())
    {
        std::vector<DWORD> rewritten;
        bool left = false;
        if (CompilerReady())
            rewritten = Build(code, tokens, kind, left);
        cached = m_cache.emplace(key, std::move(rewritten)).first;
    }
    const bool injected = !cached->second.empty();
    if (kind == SnowShaderKind::Vertex)
        (injected ? m_vertexInjected : m_vertexFailed) += 1;
    else if (kind == SnowShaderKind::Pixel)
        (injected ? m_pixelInjected : m_pixelLeft) += 1;
    return injected ? cached->second.data() : code;
}

SnowShaderRewriter& GlobalSnowShaders()
{
    static SnowShaderRewriter shaders;
    return shaders;
}

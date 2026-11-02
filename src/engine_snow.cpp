#include "engine_snow.h"

#include "client_memory.h"
#include "engine_terrain.h"
#include "log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
struct ShaderTable
{
    uintptr_t address;
    int count;
};

constexpr ShaderTable kTerrainVertexShaders = {0x00CE0008, 128};
constexpr ShaderTable kTerrainPixelShaders[] = {
    {0x00CE0488, 3}, {0x00CE0004, 1}, {0x00CE0408, 32}, {0x00CE0388, 32}, {0x00CE0208, 96},
};
constexpr uintptr_t kShaderCode = 0x50;

constexpr uintptr_t kVertexShadowConstants = 0x00C5EFE8;
constexpr uintptr_t kVertexShadowDirtyMax = 0x00C5FFE8;
constexpr uintptr_t kVertexShadowDirtyMin = 0x00C5FFEC;
constexpr unsigned kShadowRegisters = 256;
constexpr size_t kRegisterBytes = 4 * sizeof(float);

constexpr uintptr_t kChunkCorner = 0x7C;
constexpr uintptr_t kChunkHeader = 0x110;
constexpr uintptr_t kChunkHeights = 0x11C;
constexpr uintptr_t kChunkLayers = 0x12C;
constexpr uintptr_t kChunkAlpha = 0x130;
constexpr uintptr_t kHeaderFlags = 0x00;
constexpr uintptr_t kHeaderLayerCount = 0x0C;
constexpr uintptr_t kHeaderAlphaSize = 0x28;
constexpr uint32_t kHeaderAlphaFixed = 0x8000;
constexpr uintptr_t kLayerStride = 0x10;
constexpr uintptr_t kLayerTexture = 0x00;
constexpr uintptr_t kLayerFlags = 0x04;
constexpr uintptr_t kLayerAlphaOffset = 0x08;
constexpr uintptr_t kLayerEffect = 0x0C;
constexpr uintptr_t kAreaTextureCount = 0x5C;
constexpr uintptr_t kAreaTextures = 0x60;
constexpr uintptr_t kAreaTextureStride = 0x08;
constexpr uintptr_t kEffectMaxId = 0x00AD3AF4;
constexpr uintptr_t kEffectMinId = 0x00AD3AF8;
constexpr uintptr_t kEffectRecords = 0x00AD3B08;
constexpr uintptr_t kEffectTerrainType = 0x28;
constexpr uintptr_t kMapHeaderFlags = 0x00CF08D0;
constexpr uint32_t kMapEightBitAlpha = 0x4;
constexpr size_t kIffHeaderBytes = 8;
constexpr size_t kUnknownAlphaBytes = 2 * kSnowAlphaTexels;
constexpr int kChunkHeightStride = 17;
constexpr int kChunkHeightCount = 145;
constexpr int kChunkCentreOffset = 9;
constexpr size_t kMaxTexturePath = 260;
constexpr int kNoTerrainType = -1;
constexpr int kFullWeight = 255;
constexpr uintptr_t kIdentityMix = 0x9E3779B1u;

const ClientCodeGuard kSnowCode[] = {
    {"terrain vertex shader table", 0x0079E9A2, 7, {0x68, 0x08, 0x00, 0xCE, 0x00, 0xFF, 0xD2}},
    {"Terrain0 pixel shader table", 0x0079E9C5, 7, {0x68, 0x88, 0x04, 0xCE, 0x00, 0xFF, 0xD2}},
    {"Terrain0_env pixel shader", 0x0079E9E8, 7, {0x68, 0x04, 0x00, 0xCE, 0x00, 0xFF, 0xD2}},
    {"Terrain1 pixel shader table", 0x0079EA36, 7, {0x68, 0x08, 0x04, 0xCE, 0x00, 0xFF, 0xD0}},
    {"Terrain2 pixel shader table", 0x0079E574, 7, {0x68, 0x88, 0x03, 0xCE, 0x00, 0xFF, 0xD2}},
    {"Terrain3 pixel shader table", 0x0079E5A0, 7, {0x68, 0x08, 0x02, 0xCE, 0x00, 0xFF, 0xD2}},
    {"vertex shader create bytecode", 0x006AA0E7, 16,
     {0x83, 0x7E, 0x4C, 0x00, 0xC7, 0x46, 0x30, 0x01, 0x00, 0x00, 0x00, 0x74, 0x2C, 0x8B, 0x56, 0x50}},
    {"pixel shader create bytecode", 0x006AA087, 16,
     {0x83, 0x7E, 0x4C, 0x00, 0xC7, 0x46, 0x30, 0x01, 0x00, 0x00, 0x00, 0x74, 0x2C, 0x8B, 0x56, 0x50}},
    {"shader constant flush", 0x006A9FE0, 14,
     {0xA1, 0xE8, 0xFF, 0xC5, 0x00, 0x56, 0x8B, 0xF1, 0x8B, 0x0D, 0xEC, 0xFF, 0xC5, 0x00}},
    {"chunk layer alpha", 0x007B7876, 16,
     {0x8B, 0x96, 0x2C, 0x01, 0x00, 0x00, 0xF7, 0x42, 0x04, 0x00, 0x01, 0x00, 0x00, 0x74, 0x0C, 0x8B}},
    {"chunk alpha offset", 0x007B7885, 9, {0x8B, 0xBE, 0x30, 0x01, 0x00, 0x00, 0x03, 0x7A, 0x08}},
    {"ground type layers", 0x007A0638, 6, {0x8B, 0x80, 0x2C, 0x01, 0x00, 0x00}},
    {"ground type effect", 0x007A064C, 10, {0x8B, 0x4C, 0x02, 0x0C, 0x51, 0xB9, 0x00, 0x3B, 0xAD, 0x00}},
    {"ground type field", 0x007A0667, 3, {0x8B, 0x50, 0x28}},
    {"ground effect record", 0x0065C290, 16,
     {0x55, 0x8B, 0xEC, 0x8B, 0x51, 0xF8, 0x8B, 0x45, 0x08, 0x3B, 0xC2, 0x7C, 0x11, 0x3B, 0x41, 0xF4}},
    {"eight bit alpha flag", 0x007B7A74, 7, {0xF6, 0x05, 0xD0, 0x08, 0xCF, 0x00, 0x04}},
    {"area texture list", 0x007D6D47, 3, {0x83, 0xC7, 0x58}},
    {"area texture entry", 0x007D6DAB, 16,
     {0x8B, 0x47, 0x04, 0x8B, 0x4F, 0x08, 0x8B, 0x55, 0x08, 0x8D, 0x0C, 0xC1, 0x83, 0xC0, 0x01, 0x89}},
};

bool InTable(const ShaderTable& table, uintptr_t code)
{
    for (int i = 0; i < table.count; ++i)
    {
        const uintptr_t shader = Read<uintptr_t>(table.address + i * sizeof(uintptr_t));
        if (ValidPointer(shader, kShaderCode + sizeof(uintptr_t)) && Read<uintptr_t>(shader + kShaderCode) == code)
            return true;
    }
    return false;
}

bool VertexCodeUnsafe(uintptr_t code)
{
    return InTable(kTerrainVertexShaders, code);
}

bool PixelCodeUnsafe(uintptr_t code)
{
    for (const ShaderTable& table : kTerrainPixelShaders)
        if (InTable(table, code))
            return true;
    return false;
}

int TerrainTypeOf(uint32_t effect)
{
    const int32_t minId = Read<int32_t>(kEffectMinId);
    const int32_t maxId = Read<int32_t>(kEffectMaxId);
    const uintptr_t records = Read<uintptr_t>(kEffectRecords);
    const int32_t id = static_cast<int32_t>(effect);
    if (!effect || !records || id < minId || id > maxId)
        return kNoTerrainType;
    const uintptr_t record = Read<uintptr_t>(records + (id - minId) * sizeof(uintptr_t));
    return ValidPointer(record, kEffectTerrainType + sizeof(int32_t)) ? Read<int32_t>(record + kEffectTerrainType)
                                                                      : kNoTerrainType;
}

bool TexturePath(uintptr_t area, uint32_t texture, char* out)
{
    out[0] = 0;
    if (!ValidPointer(area, kAreaTextures + sizeof(uintptr_t)) || texture >= Read<uint32_t>(area + kAreaTextureCount))
        return false;
    const uintptr_t entries = Read<uintptr_t>(area + kAreaTextures);
    if (!ValidPointer(entries, (texture + 1) * kAreaTextureStride))
        return false;
    const uintptr_t name = Read<uintptr_t>(entries + texture * kAreaTextureStride);
    if (!ValidRange(name, 1))
        return false;
    for (size_t i = 0; i + 1 < kMaxTexturePath; ++i)
    {
        out[i] = Read<char>(name + i);
        if (!out[i])
            return true;
    }
    out[kMaxTexturePath - 1] = 0;
    return true;
}

uintptr_t IdentityUnsafe(int chunkRow, int chunkCol)
{
    uintptr_t area = 0;
    const uintptr_t chunk = engine::TerrainChunkUnsafe(chunkRow, chunkCol, area);
    if (!chunk)
        return 0;
    const uintptr_t layers = Read<uintptr_t>(chunk + kChunkLayers);
    const uintptr_t alpha = Read<uintptr_t>(chunk + kChunkAlpha);
    return ((chunk * kIdentityMix) ^ layers ^ (alpha << 1)) | 1;
}

bool ReadChunkUnsafe(int chunkRow, int chunkCol, SnowChunk& out)
{
    uintptr_t area = 0;
    const uintptr_t chunk = engine::TerrainChunkUnsafe(chunkRow, chunkCol, area);
    if (!chunk)
        return false;
    const uintptr_t header = Read<uintptr_t>(chunk + kChunkHeader);
    const uintptr_t heights = Read<uintptr_t>(chunk + kChunkHeights);
    if (!ValidRange(header, kHeaderAlphaSize + sizeof(uint32_t)) ||
        !ValidRange(heights, kChunkHeightCount * sizeof(float)))
        return false;
    float h[kChunkHeightCount];
    ReadFloats(heights, h, kChunkHeightCount);
    for (int row = 0; row < kTerrainChunkVertexSide; ++row)
        for (int col = 0; col < kTerrainChunkVertexSide; ++col)
            out.heights[row][col] = h[row * kChunkHeightStride + col];
    std::memset(out.snow, 0, sizeof(out.snow));
    const uint32_t chunkFlags = Read<uint32_t>(header + kHeaderFlags);
    const int layerCount = static_cast<int>(std::min<uint32_t>(Read<uint32_t>(header + kHeaderLayerCount),
                                                               static_cast<uint32_t>(kSnowMaxLayers)));
    const uintptr_t layers = Read<uintptr_t>(chunk + kChunkLayers);
    const uintptr_t alpha = Read<uintptr_t>(chunk + kChunkAlpha);
    if (layerCount <= 0 || !ValidRange(layers, layerCount * kLayerStride))
        return true;
    const uint32_t alphaSize = Read<uint32_t>(header + kHeaderAlphaSize);
    const bool eightBit = (Read<uint32_t>(kMapHeaderFlags) & kMapEightBitAlpha) != 0;
    const bool repeatEdges = !(chunkFlags & kHeaderAlphaFixed);
    static unsigned char maps[kSnowMaxLayers][kSnowAlphaTexels];
    bool snow[kSnowMaxLayers] = {};
    bool anySnow = false;
    char path[kMaxTexturePath];
    for (int l = 0; l < layerCount; ++l)
    {
        const uintptr_t layer = layers + l * kLayerStride;
        TexturePath(area, Read<uint32_t>(layer + kLayerTexture), path);
        snow[l] = SnowLayer(TerrainTypeOf(Read<uint32_t>(layer + kLayerEffect)), path);
        anySnow = anySnow || snow[l];
        std::memset(maps[l], 0, kSnowAlphaTexels);
        const uint32_t flags = Read<uint32_t>(layer + kLayerFlags);
        if (l == 0 || !(flags & kSnowLayerHasAlpha) || !alpha)
            continue;
        const uint32_t offset = Read<uint32_t>(layer + kLayerAlphaOffset);
        size_t available = kUnknownAlphaBytes;
        if (alphaSize >= kIffHeaderBytes)
            available = alphaSize - kIffHeaderBytes > offset ? alphaSize - kIffHeaderBytes - offset : 0;
        if (!ValidRange(alpha + offset, available))
            continue;
        unsigned char decoded[kSnowAlphaTexels];
        if (DecodeTerrainAlpha(reinterpret_cast<const unsigned char*>(alpha + offset), available, flags, eightBit,
                               repeatEdges, decoded))
            std::memcpy(maps[l], decoded, kSnowAlphaTexels);
    }
    if (!anySnow)
        return true;
    for (int i = 0; i < kSnowAlphaTexels; ++i)
    {
        int painted = 0;
        int weight = 0;
        for (int l = 1; l < layerCount; ++l)
        {
            painted += maps[l][i];
            weight += snow[l] ? maps[l][i] : 0;
        }
        if (snow[0])
            weight += std::max(kFullWeight - painted, 0);
        out.snow[i] = static_cast<unsigned char>(std::min(weight, kFullWeight));
    }
    return true;
}

float PlaneHeight(const float* a, const float* b, const float* c, float u, float v)
{
    const float d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1]);
    const float wa = ((b[1] - c[1]) * (u - c[0]) + (c[0] - b[0]) * (v - c[1])) / d;
    const float wb = ((c[1] - a[1]) * (u - c[0]) + (a[0] - c[0]) * (v - c[1])) / d;
    return wa * a[2] + wb * b[2] + (1.0f - wa - wb) * c[2];
}

bool HeightUnsafe(float x, float y, float& z)
{
    const int row = TerrainChunkIndex(x);
    const int col = TerrainChunkIndex(y);
    if (row < 0 || row >= kTerrainMapChunks || col < 0 || col >= kTerrainMapChunks)
        return false;
    uintptr_t area = 0;
    const uintptr_t chunk = engine::TerrainChunkUnsafe(row, col, area);
    if (!chunk)
        return false;
    const uintptr_t heights = Read<uintptr_t>(chunk + kChunkHeights);
    if (!ValidRange(heights, kChunkHeightCount * sizeof(float)))
        return false;
    float corner[3];
    ReadFloats(chunk + kChunkCorner, corner, 3);
    const float r = (corner[0] - x) / kTerrainVertexYards;
    const float c = (corner[1] - y) / kTerrainVertexYards;
    const int cellRow = std::clamp(static_cast<int>(r), 0, kTerrainChunkCells - 1);
    const int cellCol = std::clamp(static_cast<int>(c), 0, kTerrainChunkCells - 1);
    const float u = r - cellRow;
    const float v = c - cellCol;
    float h[kChunkHeightCount];
    ReadFloats(heights, h, kChunkHeightCount);
    const float* at = h + cellRow * kChunkHeightStride + cellCol;
    const float p00[3] = {0.0f, 0.0f, at[0]};
    const float p01[3] = {0.0f, 1.0f, at[1]};
    const float p10[3] = {1.0f, 0.0f, at[kChunkHeightStride]};
    const float p11[3] = {1.0f, 1.0f, at[kChunkHeightStride + 1]};
    const float mid[3] = {0.5f, 0.5f, at[kChunkCentreOffset]};
    const float du0 = u;
    const float du1 = 1.0f - u;
    const float dv0 = v;
    const float dv1 = 1.0f - v;
    float local;
    if (du0 <= du1 && du0 <= dv0 && du0 <= dv1)
        local = PlaneHeight(p00, p01, mid, u, v);
    else if (du1 <= dv0 && du1 <= dv1)
        local = PlaneHeight(p10, p11, mid, u, v);
    else if (dv0 <= dv1)
        local = PlaneHeight(p00, p10, mid, u, v);
    else
        local = PlaneHeight(p01, p11, mid, u, v);
    z = corner[2] + local;
    return std::isfinite(z);
}

bool WriteShadowUnsafe(unsigned first, const float* data, unsigned count)
{
    std::memcpy(reinterpret_cast<void*>(kVertexShadowConstants + first * kRegisterBytes), data,
                count * kRegisterBytes);
    uint32_t& dirtyMin = *reinterpret_cast<uint32_t*>(kVertexShadowDirtyMin);
    uint32_t& dirtyMax = *reinterpret_cast<uint32_t*>(kVertexShadowDirtyMax);
    dirtyMin = std::min(dirtyMin, first);
    dirtyMax = std::max(dirtyMax, first + count - 1);
    return true;
}
}

namespace engine
{
const ClientCodeGuard* SnowGuards(size_t& count)
{
    count = sizeof(kSnowCode) / sizeof(kSnowCode[0]);
    return kSnowCode;
}

bool SnowCodeMatches(const ClientCodeView& code)
{
    for (const ClientCodeGuard& guard : kSnowCode)
        if (!ClientGuardMatches(code, guard))
        {
            VF_LOG_ERROR("snow: the client's %s at 0x%08X differs from the 12340 client; the terrain stays bare",
                         guard.name, static_cast<unsigned>(guard.address));
            return false;
        }
    return true;
}

bool IsTerrainVertexShaderCode(const void* code)
{
    __try
    {
        return code && VertexCodeUnsafe(reinterpret_cast<uintptr_t>(code));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool IsTerrainPixelShaderCode(const void* code)
{
    __try
    {
        return code && PixelCodeUnsafe(reinterpret_cast<uintptr_t>(code));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool TerrainHeightAt(float x, float y, float& z)
{
    __try
    {
        return HeightUnsafe(x, y, z);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool WriteVertexShadowConstants(unsigned first, const float* data, unsigned count)
{
    if (!count || first + count > kShadowRegisters)
        return false;
    __try
    {
        return WriteShadowUnsafe(first, data, count);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

uintptr_t ClientSnowTerrain::ChunkIdentity(int chunkRow, int chunkCol) const
{
    __try
    {
        return IdentityUnsafe(chunkRow, chunkCol);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

bool ClientSnowTerrain::ReadChunk(int chunkRow, int chunkCol, SnowChunk& out) const
{
    __try
    {
        return ReadChunkUnsafe(chunkRow, chunkCol, out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
}

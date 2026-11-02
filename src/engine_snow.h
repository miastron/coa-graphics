#pragma once

#include "client_code.h"
#include "snow_cover.h"

#include <cstddef>
#include <cstdint>

namespace engine
{
const ClientCodeGuard* SnowGuards(size_t& count);
bool SnowCodeMatches(const ClientCodeView& code);
bool IsTerrainVertexShaderCode(const void* code);
bool IsTerrainPixelShaderCode(const void* code);
bool WriteVertexShadowConstants(unsigned first, const float* data, unsigned count);
bool TerrainHeightAt(float x, float y, float& z);

class ClientSnowTerrain final : public SnowSource
{
public:
    uintptr_t ChunkIdentity(int chunkRow, int chunkCol) const override;
    bool ReadChunk(int chunkRow, int chunkCol, SnowChunk& out) const override;
};
}

#pragma once

#include "client_code.h"
#include "grass_doodads.h"

#include <cstddef>
#include <cstdint>

namespace engine
{
constexpr uintptr_t kFillGrassVertices = 0x007B1B50;
constexpr uintptr_t kFillGrassVerticesSite = 0x007B33F4;
constexpr uintptr_t kDrawGrassBatch = 0x007B3390;
constexpr uintptr_t kDrawGrassBatchSite = 0x007B36C7;
constexpr uintptr_t kGrassPassEnd = 0x00797280;
constexpr uintptr_t kGrassPassEndSite = 0x00798781;
constexpr uintptr_t kDetailDoodadList = 0x00D1C4FC;
constexpr uintptr_t kDetailDoodadListSize = 0x00D1C4F8;
constexpr uintptr_t kVertexShadowConstants = 0x00C5EFE8;
constexpr size_t kBatchInstanceCount = 0x18;
constexpr size_t kBatchInstances = 0x1C;
constexpr size_t kInstanceStride = 0x2C;
constexpr size_t kInstanceDoodadId = 0x04;
constexpr size_t kInstancePositionX = 0x08;
constexpr size_t kInstancePositionY = 0x0C;
constexpr size_t kInstanceColour = 0x28;
constexpr size_t kMaxBatchInstances = 65536;
constexpr size_t kMaxModelVertices = 8192;

struct SlotTagging
{
    GrassSlotDoodads doodads;
    uint32_t tagged;
    bool overflow;
};

const ClientCodeGuard* GrassDoodadGuards(size_t& count);
bool GrassDoodadCodeMatches(const ClientCodeView& code);
bool TagGrassSlot(uintptr_t slot, SlotTagging& out, bool randomise = false);
void UntagGrassSlot(uintptr_t slot, const SlotTagging& tagging);
bool ReadGrassDoodadBandsFrom(uintptr_t listAddress, uintptr_t listSizeAddress, uint32_t id, ModelBands& out);
bool ReadGrassDoodadBands(uint32_t id, ModelBands& out);
bool ReadVertexShadowConstants(unsigned first, unsigned count, float* out);
}

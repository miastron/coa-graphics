#include "engine_grass.h"

#include "client_memory.h"
#include "grass_density.h"
#include "log.h"

#include <windows.h>

#include <cstring>

namespace
{
constexpr size_t kEntryModel = 0x04;
constexpr size_t kModelFlags = 0x10;
constexpr uint8_t kModelLoaded = 0x01;
constexpr size_t kModelShared = 0x2C;
constexpr size_t kSharedHeader = 0x150;
constexpr size_t kSharedSkin = 0x170;
constexpr size_t kHeaderVertexCount = 0x3C;
constexpr size_t kHeaderVertices = 0x40;
constexpr size_t kSkinVertexCount = 0x04;
constexpr size_t kSkinVertexLookup = 0x08;
constexpr size_t kVertexStride = 48;
constexpr size_t kVertexHeight = 8;
constexpr size_t kVertexV = 0x24;
constexpr size_t kShadowRegisters = 256;
constexpr size_t kRegisterFloats = 4;

const ClientCodeGuard kGrassDoodadCode[] = {
    {"grass fill entry", engine::kFillGrassVertices, 16,
     {0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x48, 0x01, 0x00, 0x00, 0x53, 0x8B, 0xD9, 0x8B, 0x43, 0x0C, 0x8B}},
    {"grass draw entry", engine::kDrawGrassBatch, 16,
     {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x14, 0x56, 0x8B, 0xF1, 0x8B, 0x06, 0x85, 0xC0, 0x0F, 0x84, 0xD6}},
    {"grass pass end entry", engine::kGrassPassEnd, 16,
     {0x55, 0x8B, 0xEC, 0xA1, 0xB8, 0xB0, 0xCD, 0x00, 0x81, 0xEC, 0x84, 0x00, 0x00, 0x00, 0xA8, 0x01}},
    {"grass fill call", 0x007B33F2, 9, {0x8B, 0xCE, 0xE8, 0x57, 0xE7, 0xFF, 0xFF, 0x8B, 0x0F}},
    {"grass draw call", 0x007B36C5, 7, {0x8B, 0xCE, 0xE8, 0xC4, 0xFC, 0xFF, 0xFF}},
    {"grass batch instances", 0x007B1B7D, 16,
     {0x8B, 0x7B, 0x1C, 0xD9, 0x95, 0x70, 0xFF, 0xFF, 0xFF, 0x8B, 0xF0, 0xD9, 0xE8, 0x8B, 0x43, 0x18}},
    {"grass instance and model", 0x007B1CE0, 16,
     {0x8B, 0x4F, 0xE4, 0x8B, 0x15, 0xFC, 0xC4, 0xD1, 0x00, 0x8B, 0x04, 0x8A, 0x8B, 0x58, 0x04, 0xF6}},
    {"grass model shared and header", 0x007B1D04, 16,
     {0x8B, 0x4B, 0x2C, 0x8B, 0x58, 0x04, 0xF6, 0x43, 0x10, 0x01, 0x8B, 0x91, 0x50, 0x01, 0x00, 0x00}},
    {"grass skin and colour", 0x007B1D29, 15,
     {0x8B, 0x43, 0x2C, 0x8B, 0x80, 0x70, 0x01, 0x00, 0x00, 0x8B, 0x48, 0x04, 0x8B, 0x5F, 0x08}},
    {"grass skin vertex lookup", 0x007B2330, 16,
     {0x8B, 0x45, 0xE0, 0x8B, 0x50, 0x08, 0x0F, 0xB7, 0x04, 0x4A, 0x8B, 0x55, 0xE4, 0x8D, 0x04, 0x40}},
    {"grass model vertices", 0x007B233D, 9, {0x8D, 0x04, 0x40, 0xC1, 0xE0, 0x04, 0x03, 0x42, 0x40}},
    {"grass vertex texture coordinate", 0x007B2391, 12,
     {0x8B, 0x50, 0x20, 0x89, 0x56, 0x1C, 0x8B, 0x40, 0x24, 0x8B, 0x55, 0xE0}},
    {"grass instance stride", 0x007B257E, 13,
     {0x83, 0xC7, 0x2C, 0x83, 0x6D, 0xB8, 0x01, 0x0F, 0x85, 0x55, 0xF7, 0xFF, 0xFF}},
    {"vertex shadow constants", 0x006AA003, 16,
     {0xC1, 0xE0, 0x04, 0x05, 0xE8, 0xEF, 0xC5, 0x00, 0x50, 0x51, 0x8B, 0x8F, 0x78, 0x01, 0x00, 0x00}},
};

ModelVertex g_modelScratch[engine::kMaxModelVertices];
uint32_t g_savedColours[engine::kMaxBatchInstances];

bool TagUnsafe(uintptr_t slot, engine::SlotTagging& out, bool randomise)
{
    const uint32_t count = Read<uint32_t>(slot + engine::kBatchInstanceCount);
    const uintptr_t instances = Read<uintptr_t>(slot + engine::kBatchInstances);
    if (!instances || !count || count > engine::kMaxBatchInstances ||
        !ValidRange(instances, static_cast<size_t>(count) * engine::kInstanceStride))
        return false;
    for (uint32_t i = 0; i < count; ++i)
    {
        const uintptr_t record = instances + static_cast<uintptr_t>(i) * engine::kInstanceStride;
        const uint32_t colour = Read<uint32_t>(record + engine::kInstanceColour);
        const unsigned entry =
            GrassEntryFor(out.doodads, Read<uint32_t>(record + engine::kInstanceDoodadId), out.overflow);
        g_savedColours[i] = colour;
        uint32_t tagged = (colour & ~kGrassTagMask) | GrassEntryTag(entry);
        if (randomise)
        {
            const unsigned bits = GrassPlantBits(Read<float>(record + engine::kInstancePositionX),
                                                 Read<float>(record + engine::kInstancePositionY));
            tagged = (tagged & ~kGrassPlantMask) | (static_cast<uint32_t>(bits) << kGrassPlantShift);
        }
        *reinterpret_cast<uint32_t*>(record + engine::kInstanceColour) = tagged;
        ++out.tagged;
    }
    return true;
}

void UntagUnsafe(uintptr_t slot, uint32_t tagged)
{
    const uintptr_t instances = Read<uintptr_t>(slot + engine::kBatchInstances);
    for (uint32_t i = 0; i < tagged; ++i)
        *reinterpret_cast<uint32_t*>(instances + static_cast<uintptr_t>(i) * engine::kInstanceStride +
                                     engine::kInstanceColour) = g_savedColours[i];
}

bool ReadBandsUnsafe(uintptr_t listAddress, uintptr_t listSizeAddress, uint32_t id, ModelBands& out)
{
    const uintptr_t list = Read<uintptr_t>(listAddress);
    if (!ValidPointer(list, sizeof(uintptr_t)) || id >= Read<uint32_t>(listSizeAddress))
        return false;
    const uintptr_t entry = Read<uintptr_t>(list + static_cast<uintptr_t>(id) * sizeof(uintptr_t));
    if (!ValidPointer(entry, kEntryModel + sizeof(uintptr_t)))
        return false;
    const uintptr_t model = Read<uintptr_t>(entry + kEntryModel);
    if (!ValidPointer(model, kModelShared + sizeof(uintptr_t)) || !(Read<uint8_t>(model + kModelFlags) & kModelLoaded))
        return false;
    const uintptr_t shared = Read<uintptr_t>(model + kModelShared);
    if (!ValidPointer(shared, kSharedSkin + sizeof(uintptr_t)))
        return false;
    const uintptr_t header = Read<uintptr_t>(shared + kSharedHeader);
    const uintptr_t skin = Read<uintptr_t>(shared + kSharedSkin);
    if (!ValidPointer(header, kHeaderVertices + sizeof(uintptr_t)) ||
        !ValidPointer(skin, kSkinVertexLookup + sizeof(uintptr_t)))
        return false;
    const uint32_t modelVertices = Read<uint32_t>(header + kHeaderVertexCount);
    const uintptr_t vertices = Read<uintptr_t>(header + kHeaderVertices);
    const uint32_t count = Read<uint32_t>(skin + kSkinVertexCount);
    const uintptr_t lookup = Read<uintptr_t>(skin + kSkinVertexLookup);
    if (!count || count > engine::kMaxModelVertices || !modelVertices ||
        !ValidRange(vertices, modelVertices * kVertexStride) || !ValidRange(lookup, count * sizeof(uint16_t)))
        return false;
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint32_t index = Read<uint16_t>(lookup + i * sizeof(uint16_t));
        if (index >= modelVertices)
            return false;
        const uintptr_t vertex = vertices + static_cast<uintptr_t>(index) * kVertexStride;
        g_modelScratch[i].height = Read<float>(vertex + kVertexHeight);
        g_modelScratch[i].v = Read<float>(vertex + kVertexV);
    }
    out = GrassModelBands(g_modelScratch, count);
    return true;
}

bool ReadShadowUnsafe(unsigned first, unsigned count, float* out)
{
    std::memcpy(out,
                reinterpret_cast<const void*>(engine::kVertexShadowConstants + first * kRegisterFloats * sizeof(float)),
                count * kRegisterFloats * sizeof(float));
    return true;
}
}

namespace engine
{
const ClientCodeGuard* GrassDoodadGuards(size_t& count)
{
    count = sizeof(kGrassDoodadCode) / sizeof(kGrassDoodadCode[0]);
    return kGrassDoodadCode;
}

bool GrassDoodadCodeMatches(const ClientCodeView& code)
{
    for (const ClientCodeGuard& guard : kGrassDoodadCode)
        if (!ClientGuardMatches(code, guard))
        {
            VF_LOG_ERROR("grass doodads: the client's %s at 0x%08X differs from the 12340 client; the grass bends by "
                         "its texture coordinate alone",
                         guard.name, static_cast<unsigned>(guard.address));
            return false;
        }
    return true;
}

bool TagGrassSlot(uintptr_t slot, SlotTagging& out, bool randomise)
{
    out = {};
    __try
    {
        return TagUnsafe(slot, out, randomise);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void UntagGrassSlot(uintptr_t slot, const SlotTagging& tagging)
{
    __try
    {
        UntagUnsafe(slot, tagging.tagged);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

bool ReadGrassDoodadBandsFrom(uintptr_t listAddress, uintptr_t listSizeAddress, uint32_t id, ModelBands& out)
{
    __try
    {
        return ReadBandsUnsafe(listAddress, listSizeAddress, id, out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool ReadGrassDoodadBands(uint32_t id, ModelBands& out)
{
    return ReadGrassDoodadBandsFrom(kDetailDoodadList, kDetailDoodadListSize, id, out);
}

bool ReadVertexShadowConstants(unsigned first, unsigned count, float* out)
{
    if (first + count > kShadowRegisters)
        return false;
    __try
    {
        return ReadShadowUnsafe(first, count, out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
}

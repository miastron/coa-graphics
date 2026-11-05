#include "snow_decals.h"

#include "client_memory.h"
#include "log.h"

#include <windows.h>

namespace
{
constexpr int kChunkVertexCount = 145;
constexpr uintptr_t kBatchMatrix = 0x00;
constexpr uintptr_t kBatchVertices = 0x04;
constexpr uintptr_t kBatchHeights = 0x0C;
constexpr uintptr_t kMatrixTranslationX = 0x30;
constexpr uintptr_t kMatrixTranslationY = 0x34;
constexpr size_t kMatrixBytes = 16 * sizeof(float);
constexpr size_t kChunkVertexBytes = 3 * sizeof(float);

const ClientCodeGuard kDecalLiftCode[] = {
    {"projected normal build heights", 0x007E3006, 7, {0x39, 0x5E, 0x0C, 0x8B, 0xC8, 0x74, 0x79}},
    {"projected normal build index", 0x007E3020, 16,
     {0x8B, 0x7E, 0x10, 0x8B, 0x5E, 0x04, 0x0F, 0xB7, 0xD2, 0x0F, 0xB7, 0x14, 0x57, 0x0F, 0xB7, 0xD2}},
    {"projected normal build vertex", 0x007E3030, 6, {0x8D, 0x3C, 0x52, 0x8D, 0x3C, 0xBB}},
    {"projected normal build height", 0x007E3046, 12,
     {0x8B, 0x7E, 0x0C, 0x83, 0xC1, 0x18, 0x83, 0xC0, 0x18, 0xD9, 0x04, 0x97}},
    {"projected colour build heights", 0x007E3327, 7, {0x39, 0x5E, 0x0C, 0x8B, 0xC8, 0x74, 0x5A}},
    {"projected colour build index", 0x007E3340, 16,
     {0x8B, 0x46, 0x10, 0x8B, 0x5E, 0x04, 0x0F, 0xB7, 0xD7, 0x0F, 0xB7, 0x04, 0x50, 0x0F, 0xB7, 0xC0}},
    {"projected colour build vertex", 0x007E3350, 6, {0x8D, 0x14, 0x40, 0x8D, 0x14, 0x93}},
    {"projected colour build height", 0x007E3366, 12,
     {0x8B, 0x56, 0x0C, 0x83, 0xC7, 0x01, 0x83, 0xC1, 0x10, 0xD9, 0x04, 0x82}},
    {"projected shader effect normal build call", 0x007E3CA2, 15,
     {0x74, 0x12, 0x8B, 0x55, 0x14, 0x52, 0x8D, 0x45, 0xEC, 0x50, 0xE8, 0x1F, 0xF3, 0xFF, 0xFF}},
    {"projected shader effect colour build call", 0x007E3CBC, 16,
     {0x51, 0x8D, 0x55, 0xEC, 0x52, 0x50, 0x8B, 0xC6, 0xE8, 0x27, 0xF6, 0xFF, 0xFF, 0x83, 0xC4, 0x0C}},
    {"projected render normal build call", 0x007E41C1, 15,
     {0x74, 0x12, 0x8B, 0x4D, 0x14, 0x51, 0x8D, 0x55, 0xE8, 0x52, 0xE8, 0x00, 0xEE, 0xFF, 0xFF}},
    {"projected render colour build call", 0x007E41DB, 16,
     {0x50, 0x8D, 0x4D, 0xE8, 0x51, 0x52, 0x8B, 0xC6, 0xE8, 0x08, 0xF1, 0xFF, 0xFF, 0x83, 0xC4, 0x0C}},
};

struct DecalBatchSwap
{
    uintptr_t batch = 0;
    uintptr_t heights = 0;
    float lifted[kChunkVertexCount] = {};
};

DecalBatchSwap g_decalSwap;

bool LiftDecalBatchUnsafe(uintptr_t batch, engine::SnowLiftFn lift)
{
    const uintptr_t heights = Read<uintptr_t>(batch + kBatchHeights);
    const uintptr_t matrix = Read<uintptr_t>(batch + kBatchMatrix);
    const uintptr_t vertices = Read<uintptr_t>(batch + kBatchVertices);
    if (!heights || !ValidRange(heights, kChunkVertexCount * sizeof(float)) || !ValidPointer(matrix, kMatrixBytes) ||
        !ValidPointer(vertices, kChunkVertexCount * kChunkVertexBytes))
        return false;
    const float originX = Read<float>(matrix + kMatrixTranslationX);
    const float originY = Read<float>(matrix + kMatrixTranslationY);
    for (int k = 0; k < kChunkVertexCount; ++k)
    {
        const uintptr_t vertex = vertices + k * kChunkVertexBytes;
        g_decalSwap.lifted[k] = Read<float>(heights + k * sizeof(float)) +
                                lift(originX + Read<float>(vertex), originY + Read<float>(vertex + sizeof(float)));
    }
    g_decalSwap.batch = batch;
    g_decalSwap.heights = heights;
    *reinterpret_cast<const float**>(batch + kBatchHeights) = g_decalSwap.lifted;
    return true;
}
}

namespace engine
{
const ClientCodeGuard* DecalLiftGuards(size_t& count)
{
    count = sizeof(kDecalLiftCode) / sizeof(kDecalLiftCode[0]);
    return kDecalLiftCode;
}

bool DecalLiftCodeMatches(const ClientCodeView& code)
{
    for (const ClientCodeGuard& guard : kDecalLiftCode)
        if (!ClientGuardMatches(code, guard))
        {
            VF_LOG_ERROR("snow: the client's %s at 0x%08X differs from the 12340 client; selection and spell circles "
                         "stay under the snow",
                         guard.name, static_cast<unsigned>(guard.address));
            return false;
        }
    return true;
}

bool LiftDecalBatch(uintptr_t batch, SnowLiftFn lift)
{
    __try
    {
        return batch && LiftDecalBatchUnsafe(batch, lift);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_decalSwap.batch = 0;
        return false;
    }
}

void RestoreDecalBatch()
{
    if (!g_decalSwap.batch)
        return;
    __try
    {
        *reinterpret_cast<uintptr_t*>(g_decalSwap.batch + kBatchHeights) = g_decalSwap.heights;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_decalSwap.heights = 0;
    }
    g_decalSwap.batch = 0;
}
}

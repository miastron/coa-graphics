#pragma once

#include "client_code.h"

#include <cstddef>
#include <cstdint>

namespace engine
{
constexpr uintptr_t kProjectedNormalBuild = 0x007E2FD0;
constexpr uintptr_t kProjectedColourBuild = 0x007E32F0;
constexpr uintptr_t kProjectedNormalBuildSites[] = {0x007E3CAC, 0x007E41CB};
constexpr uintptr_t kProjectedColourBuildSites[] = {0x007E3CC4, 0x007E41E3};

using SnowLiftFn = float (*)(float x, float y);

const ClientCodeGuard* DecalLiftGuards(size_t& count);
bool DecalLiftCodeMatches(const ClientCodeView& code);
bool LiftDecalBatch(uintptr_t batch, SnowLiftFn lift);
void RestoreDecalBatch();
}

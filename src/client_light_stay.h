#pragma once

#include "client_code.h"

#include <cstddef>
#include <cstdint>

constexpr uintptr_t kStaleLightKeepBranch = 0x0081E548;
constexpr uintptr_t kPointLightEnableSetter = 0x008356F0;
constexpr unsigned char kShortJumpIfEqual = 0x74;
constexpr unsigned char kShortJump = 0xEB;
constexpr unsigned char kCallRelative = 0xE8;
constexpr size_t kStaleLightDisableBlockMinBytes = 5;
constexpr size_t kStaleLightDisableBlockMaxBytes = 0x40;
constexpr size_t kStaleLightBranchBytes = 2 + kStaleLightDisableBlockMaxBytes;

enum class LightStayOutcome : uint32_t
{
    Applied,
    AlreadyInClient,
    CodeDiffers,
    WriteFailed,
};

struct StaleLightBranch
{
    unsigned char bytes[kStaleLightBranchBytes] = {};
    size_t read = 0;
    bool disableBlockFound = false;
};

namespace engine
{
bool ReadStaleLightBranch(const ClientCodeView& code, StaleLightBranch& out);
LightStayOutcome DecideLightStay(const ClientCodeView& code);
LightStayOutcome KeepLightsOnOffScreen(const ClientCodeView& code);
const char* LightStayOutcomeName(LightStayOutcome outcome);
void DescribeStaleLightBranch(const ClientCodeView& code, char* text, size_t size);
}

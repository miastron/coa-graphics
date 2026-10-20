#include "client_light_stay.h"

#include <cstdio>
#include <cstring>

namespace
{
constexpr size_t kDescribedBranchBytes = 8;

bool CallsEnableSetter(const unsigned char* block, size_t size)
{
    for (size_t at = 0; at + 5 <= size; ++at)
    {
        if (block[at] != kCallRelative)
            continue;
        int32_t relative;
        std::memcpy(&relative, block + at + 1, sizeof(relative));
        const uintptr_t callEnd = kStaleLightKeepBranch + 2 + at + 5;
        if (callEnd + relative == kPointLightEnableSetter)
            return true;
    }
    return false;
}

bool DisableBlockSkippedBy(const StaleLightBranch& branch)
{
    const size_t skip = branch.bytes[1];
    if (skip < kStaleLightDisableBlockMinBytes || skip > kStaleLightDisableBlockMaxBytes || branch.read < 2 + skip)
        return false;
    return CallsEnableSetter(branch.bytes + 2, skip);
}
}

namespace engine
{
bool ReadStaleLightBranch(const ClientCodeView& code, StaleLightBranch& out)
{
    out = StaleLightBranch{};
    if (!ReadClientCode(code, kStaleLightKeepBranch, out.bytes, 2))
        return false;
    out.read = 2;
    const size_t skip = out.bytes[1];
    if (skip >= kStaleLightDisableBlockMinBytes && skip <= kStaleLightDisableBlockMaxBytes &&
        ReadClientCode(code, kStaleLightKeepBranch + 2, out.bytes + 2, skip))
        out.read = 2 + skip;
    out.disableBlockFound = DisableBlockSkippedBy(out);
    return true;
}

LightStayOutcome DecideLightStay(const ClientCodeView& code)
{
    StaleLightBranch branch;
    if (!ReadStaleLightBranch(code, branch) || !branch.disableBlockFound)
        return LightStayOutcome::CodeDiffers;
    if (branch.bytes[0] == kShortJump)
        return LightStayOutcome::AlreadyInClient;
    return branch.bytes[0] == kShortJumpIfEqual ? LightStayOutcome::Applied : LightStayOutcome::CodeDiffers;
}

LightStayOutcome KeepLightsOnOffScreen(const ClientCodeView& code)
{
    const LightStayOutcome decision = DecideLightStay(code);
    if (decision != LightStayOutcome::Applied)
        return decision;
    return WriteClientCode(code, kStaleLightKeepBranch, &kShortJump, 1) ? LightStayOutcome::Applied
                                                                        : LightStayOutcome::WriteFailed;
}

const char* LightStayOutcomeName(LightStayOutcome outcome)
{
    switch (outcome)
    {
    case LightStayOutcome::Applied:
        return "applied";
    case LightStayOutcome::AlreadyInClient:
        return "already in the client";
    case LightStayOutcome::CodeDiffers:
        return "client code differs";
    case LightStayOutcome::WriteFailed:
        return "client code not writable";
    }
    return "unknown";
}

void DescribeStaleLightBranch(const ClientCodeView& code, char* text, size_t size)
{
    StaleLightBranch branch;
    const bool readable = ReadStaleLightBranch(code, branch);
    size_t written = 0;
    text[0] = '\0';
    if (!readable)
    {
        std::snprintf(text, size, "unreadable");
        return;
    }
    for (size_t i = 0; i < branch.read && i < kDescribedBranchBytes && written < size; ++i)
        written += static_cast<size_t>(
            std::snprintf(text + written, size - written, i ? " %02X" : "%02X", branch.bytes[i]));
}
}

#pragma once

namespace client_light_stay_checks
{
constexpr size_t kBranchCodeBytes = 0x100;
constexpr size_t kDisableBlockBytes = 0x0C;
constexpr unsigned char kBranchFiller = 0xCC;

struct BranchCode
{
    unsigned char* bytes = nullptr;
    uintptr_t base = kStaleLightKeepBranch - 0x10;

    BranchCode()
    {
        bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, kBranchCodeBytes, MEM_COMMIT | MEM_RESERVE,
                                                         PAGE_READWRITE));
        if (bytes)
            std::memset(bytes, kBranchFiller, kBranchCodeBytes);
    }
    ~BranchCode()
    {
        if (bytes)
            VirtualFree(bytes, 0, MEM_RELEASE);
    }
    ClientCodeView View() const { return {base, bytes, kBranchCodeBytes}; }
    unsigned char* At(uintptr_t address) { return bytes + (address - base); }
    void WriteBranch(unsigned char opcode, unsigned char skip, uintptr_t callTarget)
    {
        unsigned char* branch = At(kStaleLightKeepBranch);
        branch[0] = opcode;
        branch[1] = skip;
        unsigned char* call = branch + 2 + 4;
        call[0] = kCallRelative;
        const uintptr_t callEnd = kStaleLightKeepBranch + 2 + 4 + 5;
        const int32_t relative = static_cast<int32_t>(callTarget - callEnd);
        std::memcpy(call + 1, &relative, sizeof(relative));
    }
    std::vector<unsigned char> Snapshot() const { return std::vector<unsigned char>(bytes, bytes + kBranchCodeBytes); }
};

bool OnlyBranchOpcodeChanged(const std::vector<unsigned char>& before, const BranchCode& code, unsigned char opcode)
{
    const std::vector<unsigned char> after = code.Snapshot();
    const size_t at = kStaleLightKeepBranch - code.base;
    for (size_t i = 0; i < before.size(); ++i)
        if (i != at && before[i] != after[i])
            return false;
    return after[at] == opcode;
}

void CheckClientLightStay()
{
    BranchCode stock;
    stock.WriteBranch(kShortJumpIfEqual, kDisableBlockBytes, kPointLightEnableSetter);
    const std::vector<unsigned char> stockBefore = stock.Snapshot();
    const LightStayOutcome decision = engine::DecideLightStay(stock.View());
    const bool untouchedByDecision = stock.Snapshot() == stockBefore;
    const LightStayOutcome applied = engine::KeepLightsOnOffScreen(stock.View());
    const LightStayOutcome again = engine::KeepLightsOnOffScreen(stock.View());
    Check(stock.bytes && decision == LightStayOutcome::Applied && untouchedByDecision &&
              applied == LightStayOutcome::Applied && OnlyBranchOpcodeChanged(stockBefore, stock, kShortJump) &&
              again == LightStayOutcome::AlreadyInClient,
          "the stock je over the call to the light enable setter becomes a jmp, nothing else is written, and a "
          "second run sees the jmp as already in the client");

    BranchCode patched;
    patched.WriteBranch(kShortJump, kDisableBlockBytes, kPointLightEnableSetter);
    const std::vector<unsigned char> patchedBefore = patched.Snapshot();
    const bool exePatchRecognised =
        engine::KeepLightsOnOffScreen(patched.View()) == LightStayOutcome::AlreadyInClient &&
        patched.Snapshot() == patchedBefore;

    BranchCode otherOpcode;
    otherOpcode.WriteBranch(0x90, kDisableBlockBytes, kPointLightEnableSetter);
    BranchCode otherTarget;
    otherTarget.WriteBranch(kShortJumpIfEqual, kDisableBlockBytes, kPointLightEnableSetter + 0x10);
    BranchCode tinySkip;
    tinySkip.WriteBranch(kShortJumpIfEqual, 2, kPointLightEnableSetter);
    bool refused = true;
    for (BranchCode* code : {&otherOpcode, &otherTarget, &tinySkip})
    {
        const std::vector<unsigned char> before = code->Snapshot();
        refused = refused && engine::KeepLightsOnOffScreen(code->View()) == LightStayOutcome::CodeDiffers &&
                  code->Snapshot() == before;
    }
    char described[64];
    engine::DescribeStaleLightBranch(otherOpcode.View(), described, sizeof(described));
    Check(exePatchRecognised && refused && std::strncmp(described, "90 0C", 5) == 0,
          "a client whose stale-light branch is not a je over the enable-setter call is left alone and its bytes "
          "are named, while an exe already carrying the jmp is accepted");

    void* reserved = VirtualAlloc(nullptr, kBranchCodeBytes, MEM_RESERVE, PAGE_NOACCESS);
    const ClientCodeView unmapped = {stock.base, static_cast<const unsigned char*>(reserved), kBranchCodeBytes};
    const bool guarded = reserved && engine::KeepLightsOnOffScreen(unmapped) == LightStayOutcome::CodeDiffers;
    if (reserved)
        VirtualFree(reserved, 0, MEM_RELEASE);
    Check(guarded, "an unreadable client image is reported as differing code, not a fault");
}
}

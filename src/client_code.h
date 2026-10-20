#pragma once

#include <cstddef>
#include <cstdint>

constexpr size_t kMaxClientGuardBytes = 16;

struct ClientCodeGuard
{
    const char* name;
    uintptr_t address;
    size_t size;
    unsigned char bytes[kMaxClientGuardBytes];
};

struct ClientCodeView
{
    uintptr_t base = 0;
    const unsigned char* bytes = nullptr;
    size_t size = 0;
};

namespace engine
{
const unsigned char* ClientCodeAt(const ClientCodeView& code, uintptr_t address, size_t size);
bool ClientGuardMatches(const ClientCodeView& code, const ClientCodeGuard& guard);
bool ReadClientCode(const ClientCodeView& code, uintptr_t address, unsigned char* out, size_t size);
bool WriteClientCode(const ClientCodeView& code, uintptr_t address, const unsigned char* bytes, size_t size);
}

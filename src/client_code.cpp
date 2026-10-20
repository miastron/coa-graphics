#include "client_code.h"

#include <windows.h>

#include <cstring>

namespace
{
bool GuardMatchesUnsafe(const ClientCodeView& code, const ClientCodeGuard& guard)
{
    const unsigned char* bytes = engine::ClientCodeAt(code, guard.address, guard.size);
    return bytes && std::memcmp(bytes, guard.bytes, guard.size) == 0;
}

bool ReadUnsafe(const ClientCodeView& code, uintptr_t address, unsigned char* out, size_t size)
{
    const unsigned char* bytes = engine::ClientCodeAt(code, address, size);
    if (!bytes)
        return false;
    std::memcpy(out, bytes, size);
    return true;
}

bool WriteUnsafe(const ClientCodeView& code, uintptr_t address, const unsigned char* bytes, size_t size)
{
    const unsigned char* target = engine::ClientCodeAt(code, address, size);
    if (!target)
        return false;
    auto* writable = const_cast<unsigned char*>(target);
    DWORD old;
    if (!VirtualProtect(writable, size, PAGE_EXECUTE_READWRITE, &old))
        return false;
    std::memcpy(writable, bytes, size);
    VirtualProtect(writable, size, old, &old);
    FlushInstructionCache(GetCurrentProcess(), writable, size);
    return true;
}
}

namespace engine
{
const unsigned char* ClientCodeAt(const ClientCodeView& code, uintptr_t address, size_t size)
{
    if (!code.bytes)
        return reinterpret_cast<const unsigned char*>(address);
    if (address < code.base || address - code.base > code.size || size > code.size - (address - code.base))
        return nullptr;
    return code.bytes + (address - code.base);
}

bool ClientGuardMatches(const ClientCodeView& code, const ClientCodeGuard& guard)
{
    __try
    {
        return GuardMatchesUnsafe(code, guard);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool ReadClientCode(const ClientCodeView& code, uintptr_t address, unsigned char* out, size_t size)
{
    __try
    {
        return ReadUnsafe(code, address, out, size);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool WriteClientCode(const ClientCodeView& code, uintptr_t address, const unsigned char* bytes, size_t size)
{
    __try
    {
        return WriteUnsafe(code, address, bytes, size);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
}

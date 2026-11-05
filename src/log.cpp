#include "log.h"

#include <windows.h>

#include <share.h>

namespace
{
constexpr long kMaxLogBytes = 4 * 1024 * 1024;
constexpr int kFirstLogSlot = 2;
constexpr int kLastLogSlot = 8;

FILE* g_file = nullptr;
std::string g_path;
int g_level = static_cast<int>(LogLevel::Info);
CRITICAL_SECTION g_lock;
bool g_lockReady = false;

std::string SlotLogPath(const std::string& path, int slot)
{
    const size_t folder = path.find_last_of("\\/");
    const size_t dot = path.find_last_of('.');
    const bool hasExtension = dot != std::string::npos && (folder == std::string::npos || dot > folder);
    const size_t stem = hasExtension ? dot : path.size();
    return path.substr(0, stem) + "-" + std::to_string(slot) + path.substr(stem);
}

std::FILE* OpenDenyingOtherWriters(const std::string& path)
{
    return _fsopen(path.c_str(), "w", _SH_DENYWR);
}
}

LogFile OpenLogFile(const std::string& path)
{
    if (std::FILE* file = OpenDenyingOtherWriters(path))
        return {file, path};
    for (int slot = kFirstLogSlot; slot <= kLastLogSlot; ++slot)
    {
        const std::string slotPath = SlotLogPath(path, slot);
        if (std::FILE* file = OpenDenyingOtherWriters(slotPath))
            return {file, slotPath};
    }
    return {};
}

std::string HeldLogNotice(const std::string& heldPath, const std::string& slotPath, unsigned long processId)
{
    return heldPath + " is held by another client; this client (process " + std::to_string(processId) + ") logs to " +
           slotPath;
}

void LogOpen(const char* path)
{
    if (!g_lockReady)
    {
        InitializeCriticalSection(&g_lock);
        g_lockReady = true;
    }
    if (g_file)
        return;
    const LogFile log = OpenLogFile(path);
    if (!log.file)
        return;
    g_file = log.file;
    g_path = log.path;
    if (g_path != path)
        VF_LOG_INFO("%s", HeldLogNotice(path, g_path, GetCurrentProcessId()).c_str());
}

const char* LogPath()
{
    return g_path.c_str();
}

void LogSetLevel(int level)
{
    g_level = level;
}

bool LogEnabled(LogLevel level)
{
    return g_file && static_cast<int>(level) <= g_level;
}

void LogWrite(LogLevel level, const char* fmt, ...)
{
    if (!LogEnabled(level))
        return;
    EnterCriticalSection(&g_lock);
    if (std::ftell(g_file) < kMaxLogBytes)
    {
        SYSTEMTIME t;
        GetLocalTime(&t);
        std::fprintf(g_file, "%02u:%02u:%02u.%03u ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
        va_list args;
        va_start(args, fmt);
        std::vfprintf(g_file, fmt, args);
        va_end(args);
        std::fputc('\n', g_file);
        std::fflush(g_file);
    }
    LeaveCriticalSection(&g_lock);
}

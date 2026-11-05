#pragma once

#include <cstdarg>
#include <cstdio>
#include <string>

enum class LogLevel : int { Error = 0, Info = 1, Debug = 2 };

struct LogFile
{
    std::FILE* file = nullptr;
    std::string path;
};

LogFile OpenLogFile(const std::string& path);
std::string HeldLogNotice(const std::string& heldPath, const std::string& slotPath, unsigned long processId);
void LogOpen(const char* path);
const char* LogPath();
void LogSetLevel(int level);
bool LogEnabled(LogLevel level);
void LogWrite(LogLevel level, const char* fmt, ...);

#define VF_LOG_ERROR(...) LogWrite(LogLevel::Error, __VA_ARGS__)
#define VF_LOG_INFO(...) LogWrite(LogLevel::Info, __VA_ARGS__)
#define VF_LOG_DEBUG(...) LogWrite(LogLevel::Debug, __VA_ARGS__)

#pragma once

#include <string>
#include <unordered_map>

std::string TrimmedText(const std::string& text);

class IniSection
{
public:
    IniSection(const std::string& path, const char* section);

    bool Find(const char* key, std::string& value) const;
    int Int(const char* key, int fallback) const;

private:
    std::unordered_map<std::string, std::string> m_values;
};

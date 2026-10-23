#include "ini_section.h"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>

namespace
{
constexpr const char* kBlanks = " \t\r";

std::string LowerCased(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string WithoutMatchingQuotes(const std::string& text)
{
    if (text.size() >= 2 && (text.front() == '"' || text.front() == '\'') && text.back() == text.front())
        return text.substr(1, text.size() - 2);
    return text;
}

bool IsSectionHeader(const std::string& line, std::string& name)
{
    if (line.empty() || line.front() != '[')
        return false;
    const size_t close = line.find(']');
    if (close == std::string::npos)
        return false;
    name = line.substr(1, close - 1);
    return true;
}

std::string ReadWholeFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
}

std::string TrimmedText(const std::string& text)
{
    const size_t first = text.find_first_not_of(kBlanks);
    if (first == std::string::npos)
        return std::string();
    return text.substr(first, text.find_last_not_of(kBlanks) - first + 1);
}

IniSection::IniSection(const std::string& path, const char* section)
{
    const std::string text = ReadWholeFile(path);
    const std::string wanted = LowerCased(section);
    bool inSection = false;
    bool sectionSeen = false;
    size_t start = 0;
    while (start <= text.size())
    {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        const std::string line = TrimmedText(text.substr(start, end - start));
        start = end + 1;
        std::string name;
        if (IsSectionHeader(line, name))
        {
            inSection = LowerCased(TrimmedText(name)) == wanted;
            if (inSection && sectionSeen)
                break;
            sectionSeen = sectionSeen || inSection;
            continue;
        }
        const size_t equals = line.find('=');
        if (!inSection || line.empty() || line.front() == ';' || equals == std::string::npos)
            continue;
        const std::string key = LowerCased(TrimmedText(line.substr(0, equals)));
        m_values.emplace(key, WithoutMatchingQuotes(TrimmedText(line.substr(equals + 1))));
    }
}

bool IniSection::Find(const char* key, std::string& value) const
{
    const auto found = m_values.find(LowerCased(key));
    if (found == m_values.end())
        return false;
    value = found->second;
    return true;
}

int IniSection::Int(const char* key, int fallback) const
{
    std::string text;
    if (!Find(key, text) || text.empty())
        return fallback;
    size_t at = 0;
    const bool negative = text[at] == '-';
    if (negative || text[at] == '+')
        ++at;
    uint32_t value = 0;
    for (; at < text.size() && std::isdigit(static_cast<unsigned char>(text[at])); ++at)
        value = value * 10 + static_cast<uint32_t>(text[at] - '0');
    return static_cast<int>(negative ? 0u - value : value);
}

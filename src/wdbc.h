#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class WdbcTable
{
public:
    bool Load(const std::string& path, std::string& error);
    bool Parse(const unsigned char* bytes, size_t size, std::string& error);

    uint32_t Records() const { return m_records; }
    uint32_t Fields() const { return m_fields; }
    uint32_t Raw(uint32_t record, uint32_t field) const;
    int32_t Int(uint32_t record, uint32_t field) const;
    float Float(uint32_t record, uint32_t field) const;
    const char* Text(uint32_t record, uint32_t field) const;

private:
    uint32_t m_records = 0;
    uint32_t m_fields = 0;
    uint32_t m_recordSize = 0;
    std::vector<unsigned char> m_rows;
    std::vector<char> m_strings;
};

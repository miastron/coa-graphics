#include "wdbc.h"

#include <cstdio>
#include <cstring>

namespace
{
constexpr uint32_t kWdbcMagic = 0x43424457;
constexpr size_t kHeaderBytes = 20;
constexpr uint32_t kFieldBytes = sizeof(uint32_t);
constexpr uint32_t kMaxFields = 256;
constexpr size_t kMaxFileBytes = 16u << 20;

struct Header
{
    uint32_t magic;
    uint32_t records;
    uint32_t fields;
    uint32_t recordSize;
    uint32_t stringBytes;
};

bool ReadWholeFile(const std::string& path, std::vector<unsigned char>& bytes, std::string& error)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
    {
        error = "cannot open " + path;
        return false;
    }
    unsigned char chunk[4096];
    size_t read = 0;
    while ((read = std::fread(chunk, 1, sizeof(chunk), file)) > 0 && bytes.size() <= kMaxFileBytes)
        bytes.insert(bytes.end(), chunk, chunk + read);
    std::fclose(file);
    if (bytes.size() > kMaxFileBytes)
    {
        error = path + " is too large";
        return false;
    }
    return true;
}
}

bool WdbcTable::Load(const std::string& path, std::string& error)
{
    std::vector<unsigned char> bytes;
    return ReadWholeFile(path, bytes, error) && Parse(bytes.data(), bytes.size(), error);
}

bool WdbcTable::Parse(const unsigned char* bytes, size_t size, std::string& error)
{
    m_records = m_fields = m_recordSize = 0;
    m_rows.clear();
    m_strings.clear();
    Header header;
    if (size < kHeaderBytes)
    {
        error = "shorter than a WDBC header";
        return false;
    }
    std::memcpy(&header, bytes, sizeof(header));
    if (header.magic != kWdbcMagic)
    {
        error = "not a WDBC table";
        return false;
    }
    if (header.fields == 0 || header.fields > kMaxFields || header.recordSize != header.fields * kFieldBytes)
    {
        error = "unexpected WDBC record layout";
        return false;
    }
    const uint64_t rowBytes = static_cast<uint64_t>(header.records) * header.recordSize;
    if (kHeaderBytes + rowBytes + header.stringBytes != size)
    {
        error = "WDBC size does not match its header";
        return false;
    }
    m_records = header.records;
    m_fields = header.fields;
    m_recordSize = header.recordSize;
    m_rows.assign(bytes + kHeaderBytes, bytes + kHeaderBytes + rowBytes);
    m_strings.assign(bytes + kHeaderBytes + rowBytes, bytes + size);
    m_strings.push_back('\0');
    return true;
}

uint32_t WdbcTable::Raw(uint32_t record, uint32_t field) const
{
    if (record >= m_records || field >= m_fields)
        return 0;
    uint32_t value;
    std::memcpy(&value, &m_rows[static_cast<size_t>(record) * m_recordSize + field * kFieldBytes], sizeof(value));
    return value;
}

int32_t WdbcTable::Int(uint32_t record, uint32_t field) const
{
    return static_cast<int32_t>(Raw(record, field));
}

float WdbcTable::Float(uint32_t record, uint32_t field) const
{
    const uint32_t bits = Raw(record, field);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

const char* WdbcTable::Text(uint32_t record, uint32_t field) const
{
    const uint32_t offset = Raw(record, field);
    return offset < m_strings.size() ? &m_strings[offset] : "";
}

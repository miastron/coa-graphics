#include "engine_environment.h"

#include "client_memory.h"
#include "engine.h"
#include "engine_terrain.h"
#include "log.h"

#include <windows.h>

#include <cmath>

namespace
{
constexpr size_t kAreaRowProbeBytes = engine::kAreaParentField + sizeof(uint32_t);
constexpr float kMaxRawWeather = 16.0f;

const ClientCodeGuard kWindInputCode[] = {
    {"world weather load", 0x004F8645, 8, {0xA1, 0x44, 0x75, 0xCD, 0x00, 0xD9, 0x40, 0x28}},
    {"weather intensity store", 0x007848BA, 3, {0xD9, 0x56, 0x08}},
    {"area table minimum id", 0x0077FC87, 6, {0x8B, 0x0D, 0x44, 0x31, 0xAD, 0x00}},
    {"area table maximum id", 0x0077FC94, 6, {0x3B, 0x05, 0x40, 0x31, 0xAD, 0x00}},
    {"area table rows", 0x0077FC9E, 6, {0x8B, 0x0D, 0x54, 0x31, 0xAD, 0x00}},
    {"terrain chunk area id", 0x007A0526, 6, {0x8B, 0x80, 0xB0, 0x00, 0x00, 0x00}},
};

bool ReadRawWeatherUnsafe(float& raw)
{
    const uintptr_t weather = Read<uintptr_t>(engine::kWorldWeather);
    if (!weather)
    {
        raw = 0.0f;
        return true;
    }
    if (!ValidPointer(weather, engine::kWeatherIntensity + sizeof(float)))
        return false;
    raw = Read<float>(weather + engine::kWeatherIntensity);
    return std::isfinite(raw) && raw >= 0.0f && raw <= kMaxRawWeather;
}

bool ReadRawWeather(float& raw)
{
    __try
    {
        return ReadRawWeatherUnsafe(raw);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

uint32_t AreaParentUnsafe(uint32_t area)
{
    const int32_t id = static_cast<int32_t>(area);
    const int32_t minId = Read<int32_t>(engine::kAreaTableMinId);
    const int32_t maxId = Read<int32_t>(engine::kAreaTableMaxId);
    const uintptr_t rows = Read<uintptr_t>(engine::kAreaTableRows);
    if (!rows || id < minId || id > maxId)
        return 0;
    const uintptr_t row = Read<uintptr_t>(rows + static_cast<uintptr_t>(id - minId) * sizeof(uintptr_t));
    return ValidPointer(row, kAreaRowProbeBytes) ? Read<uint32_t>(row + engine::kAreaParentField) : 0;
}

uint32_t AreaParent(uint32_t area)
{
    __try
    {
        return AreaParentUnsafe(area);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}
}

namespace engine
{
const ClientCodeGuard* WindInputGuards(size_t& count)
{
    count = sizeof(kWindInputCode) / sizeof(kWindInputCode[0]);
    return kWindInputCode;
}

bool WindInputsMatch(const ClientCodeView& code)
{
    for (const ClientCodeGuard& guard : kWindInputCode)
        if (!ClientGuardMatches(code, guard))
        {
            VF_LOG_ERROR("wind: the client's %s at 0x%08X differs from the 12340 client; the wind knows no weather or "
                         "area",
                         guard.name, static_cast<unsigned>(guard.address));
            return false;
        }
    return true;
}

bool ReadWeatherIntensity(float& intensity)
{
    float raw = 0.0f;
    if (!ReadRawWeather(raw))
        return false;
    intensity = wind::WeatherIntensity(raw);
    return true;
}

bool ReadWindPlace(const float position[3], wind::Place& out)
{
    static const bool inputsMatch = WindInputsMatch(ClientCodeView());
    out = {};
    out.mapId = ReadCurrentMap();
    if (!inputsMatch)
        return false;
    float weather = 0.0f;
    if (ReadWeatherIntensity(weather))
        out.weather = weather;
    int32_t area = 0;
    if (TerrainAreaIdAt(position[0], position[1], area) && area > 0)
        out.areaCount = wind::AreaChain(static_cast<uint32_t>(area), &AreaParent, out.areaChain, wind::kMaxAreaChain);
    return true;
}
}

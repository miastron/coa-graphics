#pragma once

#include "client_code.h"
#include "wind_model.h"

#include <cstddef>
#include <cstdint>

namespace engine
{
constexpr uintptr_t kWorldWeather = 0x00CD7544;
constexpr size_t kWeatherIntensity = 0x08;
constexpr uintptr_t kAreaTableMaxId = 0x00AD3140;
constexpr uintptr_t kAreaTableMinId = 0x00AD3144;
constexpr uintptr_t kAreaTableRows = 0x00AD3154;
constexpr size_t kAreaParentField = 0x08;

const ClientCodeGuard* WindInputGuards(size_t& count);
bool WindInputsMatch(const ClientCodeView& code);
bool ReadWeatherIntensity(float& intensity);
bool ReadWindPlace(const float position[3], wind::Place& out);
}

#pragma once

#include "client_code.h"

#include <cstdint>

constexpr uintptr_t kCameraFocus = 0x00CD7778;
constexpr uintptr_t kCameraFocusStore = 0x00780871;
constexpr uintptr_t kCameraFocusHeightStore = 0x0078087F;

namespace engine
{
bool CameraFocusReadable(const ClientCodeView& code);
}

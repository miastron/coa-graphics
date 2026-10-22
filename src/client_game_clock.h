#pragma once

#include "client_code.h"

#include <cstddef>
#include <cstdint>

constexpr uintptr_t kGameClock = 0x00D37F98;
constexpr uintptr_t kGameTimeSetTime = 0x0076D810;
constexpr size_t kGameClockMinute = 0x00;
constexpr size_t kGameClockHour = 0x04;
constexpr size_t kGameClockTimeBytes = 0x20;
constexpr size_t kGameClockMinutesPerSecond = 0x30;
constexpr size_t kGameClockFractionalMinutes = 0x34;
constexpr size_t kGameClockLastTickMs = 0x3C;
constexpr size_t kGameClockRunning = 0x40;
constexpr size_t kGameClockBaseMinuteOfDay = 0x44;
constexpr size_t kGameClockBytes = 0x48;
constexpr float kMinutesPerHour = 60.0f;
constexpr float kMinutesPerDay = 1440.0f;

inline constexpr ClientCodeGuard kGameClockGuards[] = {
    {"day progression running flag", 0x0076CFF7, 4, {0x80, 0x7E, 0x40, 0x00}},
    {"day progression last tick", 0x0076D007, 3, {0x2B, 0x46, 0x3C}},
    {"day progression speed and base", 0x0076D024, 6, {0xD8, 0x4E, 0x30, 0xD8, 0x46, 0x44}},
    {"set time prologue",
     kGameTimeSetTime,
     16,
     {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x20, 0x8B, 0x45, 0x08, 0x53, 0x56, 0x8B, 0xD9, 0x83, 0x7B, 0x24}},
    {"set time epilogue", 0x0076D8FA, 3, {0xC2, 0x08, 0x00}},
    {"update time clock address", 0x007E273F, 5, {0xB9, 0x98, 0x7F, 0xD3, 0x00}},
};

namespace engine
{
bool GameClockLayoutMatches(const ClientCodeView& code);
void PinGameClock(unsigned char* clock, float minuteOfDay);
bool ResumeGameClock(unsigned char* clock);
}

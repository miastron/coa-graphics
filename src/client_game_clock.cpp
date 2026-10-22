#include "client_game_clock.h"

#include <windows.h>

#include <cstring>

namespace
{
using SetGameTimeFn = void(__fastcall*)(void* clock, void* unusedEdx, const void* time, int tick);

template <typename T>
void Store(unsigned char* clock, size_t offset, T value)
{
    std::memcpy(clock + offset, &value, sizeof(value));
}

bool ResumeUnsafe(unsigned char* clock)
{
    unsigned char time[kGameClockTimeBytes];
    std::memcpy(time, clock, sizeof(time));
    reinterpret_cast<SetGameTimeFn>(kGameTimeSetTime)(clock, nullptr, time, 1);
    return true;
}
}

namespace engine
{
bool GameClockLayoutMatches(const ClientCodeView& code)
{
    for (const ClientCodeGuard& guard : kGameClockGuards)
        if (!ClientGuardMatches(code, guard))
            return false;
    return true;
}

void PinGameClock(unsigned char* clock, float minuteOfDay)
{
    const int wholeMinutes = static_cast<int>(minuteOfDay);
    Store<int32_t>(clock, kGameClockMinute, wholeMinutes % static_cast<int>(kMinutesPerHour));
    Store<int32_t>(clock, kGameClockHour, wholeMinutes / static_cast<int>(kMinutesPerHour));
    Store<float>(clock, kGameClockFractionalMinutes, 0.0f);
    Store<uint8_t>(clock, kGameClockRunning, 0);
    Store<float>(clock, kGameClockBaseMinuteOfDay, minuteOfDay);
}

bool ResumeGameClock(unsigned char* clock)
{
    __try
    {
        return ResumeUnsafe(clock);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
}

#include "time_of_day.h"

#include "client_game_clock.h"
#include "client_memory.h"
#include "engine.h"
#include "log.h"

#include <windows.h>

#include <cmath>

namespace
{
constexpr uintptr_t kDayFraction = 0x00D38B04;
constexpr float kHoursPerDay = 24.0f;

struct TimeOfDayState
{
    bool checked = false;
    bool available = false;
    bool frozen = false;
    float minuteOfDay = 0.0f;
    bool anchored = false;
    float anchorMinuteOfDay = 0.0f;
    ULONGLONG anchorTickMs = 0;
};

TimeOfDayState g_state;

unsigned char* GameClock()
{
    return reinterpret_cast<unsigned char*>(kGameClock);
}

bool Available()
{
    if (!g_state.checked)
    {
        g_state.checked = true;
        g_state.available = engine::IsSupportedClient() && engine::GameClockLayoutMatches({});
        if (!g_state.available)
            VF_LOG_INFO("time of day: the client's game clock code differs; the time controls are off");
    }
    return g_state.available;
}

float WrapMinuteOfDay(float minutes)
{
    const float wrapped = std::fmod(minutes, kMinutesPerDay);
    return wrapped < 0.0f ? wrapped + kMinutesPerDay : wrapped;
}

float LiveHours()
{
    return Read<float>(kDayFraction) * kHoursPerDay;
}

float MinutesPerSecond()
{
    return Read<float>(kGameClock + kGameClockMinutesPerSecond);
}

void AnchorLiveTime()
{
    g_state.anchored = true;
    g_state.anchorMinuteOfDay = LiveHours() * kMinutesPerHour;
    g_state.anchorTickMs = GetTickCount64();
}

float LiveMinuteOfDayNow()
{
    const double elapsedSeconds = (GetTickCount64() - g_state.anchorTickMs) * 0.001;
    return engine::AdvancedMinuteOfDay(g_state.anchorMinuteOfDay, MinutesPerSecond(), elapsedSeconds);
}
}

TimeOfDayStatus CurrentTimeOfDay()
{
    TimeOfDayStatus status;
    status.available = Available();
    status.frozen = g_state.frozen;
    if (status.available)
        status.hours = g_state.frozen ? g_state.minuteOfDay / kMinutesPerHour : LiveHours();
    return status;
}

void FreezeTimeOfDay(float hours)
{
    if (!Available())
        return;
    if (!g_state.frozen)
        AnchorLiveTime();
    g_state.frozen = true;
    g_state.minuteOfDay = WrapMinuteOfDay(hours * kMinutesPerHour);
    HoldTimeOfDay();
}

void ResumeTimeOfDay()
{
    if (!g_state.frozen)
        return;
    g_state.frozen = false;
    g_state.anchored = false;
    if (!engine::ResumeGameClock(GameClock()))
        VF_LOG_ERROR("time of day: could not restart the client's game clock");
}

void RestoreLiveTimeOfDay()
{
    if (!g_state.frozen)
        return;
    if (g_state.anchored)
        engine::PinGameClock(GameClock(), LiveMinuteOfDayNow());
    g_state.anchored = false;
    ResumeTimeOfDay();
}

void HoldTimeOfDay()
{
    if (g_state.frozen)
        engine::PinGameClock(GameClock(), g_state.minuteOfDay);
}

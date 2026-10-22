#include "time_of_day.h"

#include "client_game_clock.h"
#include "client_memory.h"
#include "engine.h"
#include "log.h"

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
    g_state.frozen = true;
    g_state.minuteOfDay = WrapMinuteOfDay(hours * kMinutesPerHour);
    HoldTimeOfDay();
}

void ResumeTimeOfDay()
{
    if (!g_state.frozen)
        return;
    g_state.frozen = false;
    if (!engine::ResumeGameClock(GameClock()))
        VF_LOG_ERROR("time of day: could not restart the client's game clock");
}

void HoldTimeOfDay()
{
    if (g_state.frozen)
        engine::PinGameClock(GameClock(), g_state.minuteOfDay);
}

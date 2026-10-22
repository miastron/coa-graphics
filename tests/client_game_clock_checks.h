#pragma once

namespace client_game_clock_checks
{
constexpr uintptr_t kImageBase = kGameClockGuards[0].address - 0x10;
constexpr size_t kImageBytes = 0x76000;
constexpr float kStockMinutesPerSecond = 1.0f / 60.0f;
constexpr uint32_t kLastTickMs = 5000;
constexpr float kPinnedMinuteOfDay = 13.0f * 60.0f + 30.5f;

struct ClockImage
{
    std::vector<unsigned char> bytes = std::vector<unsigned char>(kImageBytes, 0);

    ClockImage()
    {
        for (const ClientCodeGuard& guard : kGameClockGuards)
            std::memcpy(bytes.data() + (guard.address - kImageBase), guard.bytes, guard.size);
    }
    ClientCodeView View() const { return {kImageBase, bytes.data(), bytes.size()}; }
};

template <typename T>
T Load(const unsigned char* clock, size_t offset)
{
    T value;
    std::memcpy(&value, clock + offset, sizeof(value));
    return value;
}

template <typename T>
void Put(unsigned char* clock, size_t offset, T value)
{
    std::memcpy(clock + offset, &value, sizeof(value));
}

float DayProgression(const unsigned char* clock, uint32_t nowMs)
{
    const uint32_t elapsed =
        Load<uint8_t>(clock, kGameClockRunning) ? nowMs - Load<uint32_t>(clock, kGameClockLastTickMs) : 0;
    float minutes = elapsed * 0.001f * Load<float>(clock, kGameClockMinutesPerSecond) +
                    Load<float>(clock, kGameClockBaseMinuteOfDay);
    while (minutes > kMinutesPerDay)
        minutes -= kMinutesPerDay;
    return minutes / kMinutesPerDay;
}

void CheckGameClockGuards()
{
    ClockImage stock;
    const bool stockAccepted = engine::GameClockLayoutMatches(stock.View());
    bool everyByteMatters = true;
    for (const ClientCodeGuard& guard : kGameClockGuards)
    {
        ClockImage changed;
        changed.bytes[guard.address - kImageBase + guard.size - 1] ^= 0xFF;
        everyByteMatters = everyByteMatters && !engine::GameClockLayoutMatches(changed.View());
    }
    Check(stockAccepted && everyByteMatters,
          "the game clock code is accepted only while every recorded byte of the day progression and set time "
          "functions matches");
}

void CheckPinnedClockHoldsTheTime()
{
    std::vector<unsigned char> clock(kGameClockBytes, 0);
    Put<int32_t>(clock.data(), kGameClockMinute, 59);
    Put<int32_t>(clock.data(), kGameClockHour, 23);
    Put<float>(clock.data(), kGameClockMinutesPerSecond, kStockMinutesPerSecond);
    Put<float>(clock.data(), kGameClockFractionalMinutes, 0.7f);
    Put<uint32_t>(clock.data(), kGameClockLastTickMs, kLastTickMs);
    Put<uint8_t>(clock.data(), kGameClockRunning, 1);
    Put<float>(clock.data(), kGameClockBaseMinuteOfDay, 100.0f);

    engine::PinGameClock(clock.data(), kPinnedMinuteOfDay);
    const float expected = kPinnedMinuteOfDay / kMinutesPerDay;
    const float justAfter = DayProgression(clock.data(), kLastTickMs + 1);
    const float hoursLater = DayProgression(clock.data(), kLastTickMs + 3600u * 1000u);
    Check(Load<int32_t>(clock.data(), kGameClockHour) == 13 && Load<int32_t>(clock.data(), kGameClockMinute) == 30 &&
              std::fabs(justAfter - expected) < 1e-6f && std::fabs(hoursLater - expected) < 1e-6f,
          "a pinned game clock reads the chosen hour and minute and its day progression no longer advances with "
          "the tick time");
    Check(Load<float>(clock.data(), kGameClockMinutesPerSecond) == kStockMinutesPerSecond &&
              Load<uint32_t>(clock.data(), kGameClockLastTickMs) == kLastTickMs,
          "pinning the game clock leaves its speed and last tick time alone");

    engine::PinGameClock(clock.data(), kMinutesPerDay - 0.5f);
    Check(Load<int32_t>(clock.data(), kGameClockHour) == 23 && Load<int32_t>(clock.data(), kGameClockMinute) == 59 &&
              DayProgression(clock.data(), kLastTickMs) < 1.0f,
          "the last half minute of the day pins as 23:59, not as the next day");
}

void CheckGameClock()
{
    CheckGameClockGuards();
    CheckPinnedClockHoldsTheTime();
}
}

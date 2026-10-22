#pragma once

struct TimeOfDayStatus
{
    bool available = false;
    bool frozen = false;
    float hours = 0.0f;
};

TimeOfDayStatus CurrentTimeOfDay();
void FreezeTimeOfDay(float hours);
void ResumeTimeOfDay();
void HoldTimeOfDay();

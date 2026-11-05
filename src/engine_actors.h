#pragma once

#include <cstdint>

constexpr uint32_t kMaxWaterContacts = 32;
constexpr float kWaterContactRange = 48.0f;
constexpr uint32_t kMaxNearbyUnits = 16;

struct WaterContact
{
    uint64_t guid = 0;
    float position[3] = {};
    float surface = 0.0f;
    float radius = 0.0f;
    float height = 0.0f;
    bool swimming = false;
    bool onTransport = false;
};

struct WaterContactFrame
{
    WaterContact contacts[kMaxWaterContacts] = {};
    uint32_t count = 0;
    bool truncated = false;
};

struct NearbyUnit
{
    uint64_t guid = 0;
    float position[3] = {};
    float radius = 0.0f;
    bool mounted = false;
    float facing = 0.0f;
    bool fourLegged = false;
};

constexpr int kCreatureTypeBeast = 1;
constexpr int kCreatureTypeCritter = 8;

bool FourLeggedCreature(int creatureType, uint32_t creatureFamily);

struct NearbyUnitFrame
{
    NearbyUnit units[kMaxNearbyUnits] = {};
    uint32_t count = 0;
    bool truncated = false;
};

float WaterContactDepth(const WaterContact& contact);
float ClientRippleDepthLimit(float height);

namespace engine
{
bool SelectWaterContact(WaterContactFrame& frame, const WaterContact& contact, const float centre[3]);
bool CaptureWaterContactsFrom(uintptr_t objectManager, const float centre[3], WaterContactFrame& out);
bool WaterContactsSupported();
bool CaptureWaterContacts(const float centre[3], WaterContactFrame& out);
bool SelectNearbyUnit(NearbyUnitFrame& frame, const NearbyUnit& unit, const float centre[3], float range);
bool CaptureNearbyUnitsFrom(uintptr_t objectManager, const float centre[3], float range, NearbyUnitFrame& out);
bool NearbyUnitsSupported();
bool CaptureNearbyUnits(const float centre[3], float range, NearbyUnitFrame& out);
bool ReadActivePlayerPosition(float out[3]);
}

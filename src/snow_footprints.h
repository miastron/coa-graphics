#pragma once

#include "engine_actors.h"
#include "snow_trenches.h"

#include <cstdint>
#include <unordered_map>

constexpr float kFootprintHumanRadius = 0.306f;
constexpr float kFootprintMinSize = 0.5f;
constexpr float kFootprintMaxSize = 3.0f;
constexpr float kFootprintMountSize = 1.6f;
constexpr float kFootprintJumpYards = 3.0f;
constexpr float kFootprintMaxSpeed = 40.0f;
constexpr float kFootprintWalkSpeed = 2.5f;
constexpr float kFootprintRunSpeed = 7.0f;

struct FootprintGait
{
    float size;
    bool fourLegs;
    float halfLength;
    float halfWidth;
    float halfTrack;
    float halfBody;
    float hindTrackShare;
    float strideBase;
    float stridePerSpeed;
};

FootprintGait GaitFor(const NearbyUnit& unit);
float StepLength(const FootprintGait& gait, float speed);
float DragShare(float speed);

struct FootprintWalker
{
    float position[2];
    float travelled;
    float speed;
    float lastPrint[4][2];
    bool hasPrint[4];
    unsigned steps;
    bool seen;
};

class FootprintStamper
{
public:
    unsigned Step(TrenchMap& map, const NearbyUnitFrame& units, const bool* onGround, float seconds);
    void Reset() { m_walkers.clear(); }

private:
    void Stand(TrenchMap& map, const NearbyUnit& unit, const FootprintGait& gait, FootprintWalker& walker);
    void Plant(TrenchMap& map, const NearbyUnit& unit, const FootprintGait& gait, FootprintWalker& walker,
               const float* point, const float* direction);
    void PrintFoot(TrenchMap& map, const NearbyUnit& unit, const FootprintGait& gait, FootprintWalker& walker,
                   int foot, const float* centre, float stepLength);

    std::unordered_map<uint64_t, FootprintWalker> m_walkers;
};

FootprintStamper& GlobalFootprintStamper();

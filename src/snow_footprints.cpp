#include "snow_footprints.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kBipedHalfLength = 0.17f;
constexpr float kBipedHalfWidth = 0.08f;
constexpr float kBipedHalfTrack = 0.11f;
constexpr float kBipedStrideBase = 0.45f;
constexpr float kBipedStridePerSpeed = 0.11f;
constexpr float kHoofRadius = 0.075f;
constexpr float kQuadrupedHalfTrack = 0.13f;
constexpr float kQuadrupedHalfBody = 0.45f;
constexpr float kQuadrupedHindTrackShare = 0.8f;
constexpr float kQuadrupedStrideBase = 0.4f;
constexpr float kQuadrupedStridePerSpeed = 0.1f;
constexpr float kScuffWidthShare = 0.45f;
constexpr float kScuffWalkShare = 0.4f;
constexpr float kScuffRunShare = 0.15f;
constexpr float kScuffReachSteps = 3.0f;
constexpr float kMinimumMove = 1e-4f;
constexpr uint64_t kHashMultiplier = 0x9E3779B97F4A7C15ull;
constexpr int kHashShift = 56;

uint8_t FadeByte(uint64_t guid, unsigned steps, int foot)
{
    uint64_t h = (guid ^ (static_cast<uint64_t>(steps) << 3 | static_cast<uint64_t>(foot))) * kHashMultiplier;
    h ^= h >> 29;
    h *= kHashMultiplier;
    return static_cast<uint8_t>(h >> kHashShift);
}
}

FootprintGait GaitFor(const NearbyUnit& unit)
{
    float size = std::clamp(unit.radius / kFootprintHumanRadius, kFootprintMinSize, kFootprintMaxSize);
    if (unit.mounted)
        size = std::max(size, kFootprintMountSize);
    FootprintGait gait = {};
    gait.size = size;
    gait.fourLegs = unit.mounted || unit.fourLegged;
    if (gait.fourLegs)
    {
        gait.halfLength = size * kHoofRadius;
        gait.halfWidth = size * kHoofRadius;
        gait.halfTrack = size * kQuadrupedHalfTrack;
        gait.halfBody = size * kQuadrupedHalfBody;
        gait.hindTrackShare = kQuadrupedHindTrackShare;
        gait.strideBase = size * kQuadrupedStrideBase;
        gait.stridePerSpeed = size * kQuadrupedStridePerSpeed;
    }
    else
    {
        gait.halfLength = size * kBipedHalfLength;
        gait.halfWidth = size * kBipedHalfWidth;
        gait.halfTrack = size * kBipedHalfTrack;
        gait.strideBase = size * kBipedStrideBase;
        gait.stridePerSpeed = size * kBipedStridePerSpeed;
    }
    return gait;
}

float StepLength(const FootprintGait& gait, float speed)
{
    return gait.strideBase + gait.stridePerSpeed * std::clamp(speed, 0.0f, kFootprintMaxSpeed);
}

float DragShare(float speed)
{
    const float t =
        std::clamp((speed - kFootprintWalkSpeed) / (kFootprintRunSpeed - kFootprintWalkSpeed), 0.0f, 1.0f);
    return kScuffWalkShare + (kScuffRunShare - kScuffWalkShare) * t;
}

void FootprintStamper::PrintFoot(TrenchMap& map, const NearbyUnit& unit, const FootprintGait& gait,
                                 FootprintWalker& walker, int foot, const float* centre, float stepLength)
{
    const uint8_t fade = FadeByte(unit.guid, walker.steps, foot);
    if (walker.hasPrint[foot])
    {
        const float dx = centre[0] - walker.lastPrint[foot][0];
        const float dy = centre[1] - walker.lastPrint[foot][1];
        const float reach = kScuffReachSteps * 2.0f * stepLength;
        if (dx * dx + dy * dy <= reach * reach)
            map.StampScuff({{walker.lastPrint[foot][0], walker.lastPrint[foot][1]},
                            {centre[0], centre[1]},
                            kScuffWidthShare * gait.halfWidth,
                            DragShare(walker.speed),
                            fade});
    }
    map.StampPrint({{centre[0], centre[1]}, unit.facing, gait.halfLength, gait.halfWidth, 1.0f, fade});
    walker.lastPrint[foot][0] = centre[0];
    walker.lastPrint[foot][1] = centre[1];
    walker.hasPrint[foot] = true;
}

void FootprintStamper::Plant(TrenchMap& map, const NearbyUnit& unit, const FootprintGait& gait,
                             FootprintWalker& walker, const float* point, const float* direction)
{
    const float side = walker.steps % 2 == 0 ? 1.0f : -1.0f;
    const float lateral[2] = {-direction[1] * side, direction[0] * side};
    const float stepLength = StepLength(gait, walker.speed);
    if (gait.fourLegs)
    {
        const float front[2] = {point[0] + direction[0] * gait.halfBody + lateral[0] * gait.halfTrack,
                                point[1] + direction[1] * gait.halfBody + lateral[1] * gait.halfTrack};
        const float hindTrack = gait.halfTrack * gait.hindTrackShare;
        const float hind[2] = {point[0] - direction[0] * gait.halfBody - lateral[0] * hindTrack,
                               point[1] - direction[1] * gait.halfBody - lateral[1] * hindTrack};
        PrintFoot(map, unit, gait, walker, side > 0.0f ? 0 : 1, front, stepLength);
        PrintFoot(map, unit, gait, walker, side > 0.0f ? 3 : 2, hind, stepLength);
    }
    else
    {
        const float foot[2] = {point[0] + lateral[0] * gait.halfTrack, point[1] + lateral[1] * gait.halfTrack};
        PrintFoot(map, unit, gait, walker, side > 0.0f ? 0 : 1, foot, stepLength);
    }
    ++walker.steps;
}

void FootprintStamper::Stand(TrenchMap& map, const NearbyUnit& unit, const FootprintGait& gait,
                             FootprintWalker& walker)
{
    const float direction[2] = {std::cos(unit.facing), std::sin(unit.facing)};
    const float here[2] = {unit.position[0], unit.position[1]};
    for (bool& has : walker.hasPrint)
        has = false;
    Plant(map, unit, gait, walker, here, direction);
    Plant(map, unit, gait, walker, here, direction);
    walker.travelled = 0.0f;
}

unsigned FootprintStamper::Step(TrenchMap& map, const NearbyUnitFrame& units, const bool* onGround, float seconds)
{
    for (auto& walker : m_walkers)
        walker.second.seen = false;
    unsigned stamps = 0;
    for (uint32_t i = 0; i < units.count; ++i)
    {
        const NearbyUnit& unit = units.units[i];
        if (!onGround[i])
        {
            m_walkers.erase(unit.guid);
            continue;
        }
        const FootprintGait gait = GaitFor(unit);
        const float here[2] = {unit.position[0], unit.position[1]};
        auto found = m_walkers.find(unit.guid);
        const bool known = found != m_walkers.end();
        FootprintWalker& walker = m_walkers[unit.guid];
        walker.seen = true;
        const float dx = known ? here[0] - walker.position[0] : 0.0f;
        const float dy = known ? here[1] - walker.position[1] : 0.0f;
        const float moved = std::sqrt(dx * dx + dy * dy);
        if (!known || moved > kFootprintJumpYards)
        {
            walker = {{here[0], here[1]}, 0.0f, 0.0f, {}, {}, 0, true};
            Stand(map, unit, gait, walker);
            ++stamps;
            continue;
        }
        if (moved < kMinimumMove)
            continue;
        if (seconds > 0.0f)
            walker.speed = std::min(moved / seconds, kFootprintMaxSpeed);
        const float direction[2] = {dx / moved, dy / moved};
        const float stepLength = StepLength(gait, walker.speed);
        float along = 0.0f;
        while (walker.travelled + (moved - along) >= stepLength)
        {
            along += stepLength - walker.travelled;
            walker.travelled = 0.0f;
            const float point[2] = {walker.position[0] + direction[0] * along,
                                    walker.position[1] + direction[1] * along};
            Plant(map, unit, gait, walker, point, direction);
            ++stamps;
        }
        walker.travelled += moved - along;
        walker.position[0] = here[0];
        walker.position[1] = here[1];
    }
    for (auto it = m_walkers.begin(); it != m_walkers.end();)
        it = it->second.seen ? std::next(it) : m_walkers.erase(it);
    return stamps;
}

FootprintStamper& GlobalFootprintStamper()
{
    static FootprintStamper stamper;
    return stamper;
}

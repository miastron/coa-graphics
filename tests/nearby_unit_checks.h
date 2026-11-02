#pragma once

namespace nearby_unit_checks
{
constexpr size_t kUnitBytes = 0xA40;
constexpr uintptr_t kMountState = 0x9C0;
constexpr uintptr_t kClientFlags = 0xA30;
constexpr uint32_t kSuppressedMount = 0x10000000;
constexpr float kRange = 40.0f;

struct Fixture
{
    uint64_t guid = 0;
    float position[3] = {};
    float entityPosition[3] = {};
    float radius = 0.35f;
    int32_t mountState = 0;
    uint32_t clientFlags = 0;
    uint32_t typeMask = water_contact_checks::kUnitTypeMask;
    uint64_t transportGuid = 0;
    bool movementInBlock = true;
    bool entity = true;
};

Fixture UnitAt(uint64_t guid, float x, float y)
{
    Fixture unit;
    unit.guid = guid;
    unit.position[0] = x;
    unit.position[1] = y;
    unit.position[2] = 5.0f;
    return unit;
}

void Add(water_contact_checks::SyntheticObjects& world, const Fixture& unit)
{
    using water_contact_checks::Put;
    const uintptr_t object = world.AddObject(kUnitBytes, unit.typeMask);
    Put<uint64_t>(object + water_contact_checks::kObjectGuid, unit.guid);
    Put<uintptr_t>(object + water_contact_checks::kUnitMovement,
                   unit.movementInBlock ? object + water_contact_checks::kUnitMovementBlock : object);
    Put<uint64_t>(object + water_contact_checks::kUnitTransportGuid, unit.transportGuid);
    std::memcpy(reinterpret_cast<void*>(object + water_contact_checks::kUnitPosition), unit.position,
                sizeof(unit.position));
    Put<float>(object + water_contact_checks::kUnitRadiusField, unit.radius);
    Put<int32_t>(object + kMountState, unit.mountState);
    Put<uint32_t>(object + kClientFlags, unit.clientFlags);
    if (!unit.transportGuid || !unit.entity)
        return;
    const uintptr_t entity = world.AddObject(water_contact_checks::kEntityBytes, 0);
    std::memcpy(reinterpret_cast<void*>(entity + water_contact_checks::kEntityPosition), unit.entityPosition,
                sizeof(unit.entityPosition));
    Put<uintptr_t>(object + water_contact_checks::kObjectWorldEntity, entity);
}

const NearbyUnit* UnitOf(const NearbyUnitFrame& frame, uint64_t guid)
{
    for (uint32_t i = 0; i < frame.count; ++i)
        if (frame.units[i].guid == guid)
            return &frame.units[i];
    return nullptr;
}

void CheckSelection()
{
    const float centre[3] = {0.0f, 0.0f, 5.0f};
    NearbyUnitFrame frame;
    const NearbyUnit distant = {1, {30.0f, 0.0f, 5.0f}, 0.5f, false};
    const NearbyUnit close = {2, {3.0f, 4.0f, 5.0f}, 0.5f, false};
    const NearbyUnit tie = {3, {-3.0f, 4.0f, 5.0f}, 0.5f, false};
    const NearbyUnit earlierTie = {0, {5.0f, 0.0f, 5.0f}, 0.5f, false};
    const bool selected = engine::SelectNearbyUnit(frame, distant, centre, kRange) &&
                          engine::SelectNearbyUnit(frame, close, centre, kRange) &&
                          engine::SelectNearbyUnit(frame, tie, centre, kRange) &&
                          engine::SelectNearbyUnit(frame, earlierTie, centre, kRange);
    Check(selected && frame.count == 4 && frame.units[0].guid == 0 && frame.units[1].guid == 2 &&
              frame.units[2].guid == 3 && frame.units[3].guid == 1,
          "nearby units are kept nearest first, equally close ones in guid order, by ground distance");

    NearbyUnitFrame limited;
    const NearbyUnit beyond = {4, {kRange + 0.5f, 0.0f, 5.0f}, 0.5f, false};
    const NearbyUnit edge = {5, {kRange, 0.0f, 500.0f}, 0.5f, false};
    const bool rangeKept = !engine::SelectNearbyUnit(limited, beyond, centre, kRange) &&
                           engine::SelectNearbyUnit(limited, edge, centre, kRange);
    Check(rangeKept && limited.count == 1, "a unit counts within the range on the ground plane, whatever its height");

    NearbyUnitFrame rejected;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const NearbyUnit badPosition = {6, {nan, 0.0f, 5.0f}, 0.5f, false};
    const NearbyUnit noRadius = {7, {1.0f, 0.0f, 5.0f}, 0.0f, false};
    const NearbyUnit hugeRadius = {8, {1.0f, 0.0f, 5.0f}, 500.0f, false};
    const NearbyUnit nanRadius = {9, {1.0f, 0.0f, 5.0f}, nan, false};
    const NearbyUnit farAway = {10, {1.0e7f, 0.0f, 5.0f}, 0.5f, false};
    const float badCentre[3] = {nan, 0.0f, 0.0f};
    Check(!engine::SelectNearbyUnit(rejected, badPosition, centre, kRange) &&
              !engine::SelectNearbyUnit(rejected, noRadius, centre, kRange) &&
              !engine::SelectNearbyUnit(rejected, hugeRadius, centre, kRange) &&
              !engine::SelectNearbyUnit(rejected, nanRadius, centre, kRange) &&
              !engine::SelectNearbyUnit(rejected, farAway, centre, kRange) &&
              !engine::SelectNearbyUnit(rejected, close, badCentre, kRange) && rejected.count == 0,
          "units with a position or collision radius that is not plausible, and a centre that is not, are refused");

    NearbyUnitFrame crowd;
    for (uint32_t i = 0; i < kMaxNearbyUnits + 6; ++i)
        engine::SelectNearbyUnit(crowd, {100 + i, {1.0f + i, 0.0f, 5.0f}, 0.5f, false}, centre, kRange);
    const bool nearestKept = crowd.count == kMaxNearbyUnits && crowd.units[0].guid == 100 &&
                             crowd.units[kMaxNearbyUnits - 1].guid == 100 + kMaxNearbyUnits - 1;
    const bool lateNearOne = engine::SelectNearbyUnit(crowd, {1, {0.5f, 0.0f, 5.0f}, 0.5f, false}, centre, kRange);
    Check(nearestKept && lateNearOne && crowd.count == kMaxNearbyUnits && crowd.units[0].guid == 1 &&
              crowd.units[kMaxNearbyUnits - 1].guid == 100 + kMaxNearbyUnits - 2,
          "a crowd keeps its sixteen nearest units and a nearer latecomer pushes the farthest out");
}

void CheckVisibleUnitWalk()
{
    using water_contact_checks::SyntheticObjects;
    const float centre[3] = {0.0f, 0.0f, 5.0f};
    SyntheticObjects world;
    Fixture rider = UnitAt(1, 6.0f, 0.0f);
    rider.mountState = 3;
    Add(world, rider);
    Fixture grounded = UnitAt(2, -4.0f, 3.0f);
    grounded.radius = 0.9f;
    Add(world, grounded);
    Fixture player = UnitAt(3, 1.0f, 1.0f);
    player.typeMask = water_contact_checks::kPlayerTypeMask;
    player.mountState = 3;
    player.clientFlags = kSuppressedMount;
    Add(world, player);
    Fixture passenger = UnitAt(4, 500.0f, 500.0f);
    passenger.transportGuid = 0xF120000000001234ull;
    passenger.entityPosition[0] = 8.0f;
    passenger.entityPosition[1] = -2.0f;
    passenger.entityPosition[2] = 9.0f;
    Add(world, passenger);
    Fixture gameObject = UnitAt(5, 2.0f, 2.0f);
    gameObject.typeMask = water_contact_checks::kGameObjectTypeMask;
    Add(world, gameObject);
    Fixture detached = UnitAt(6, 2.0f, 2.0f);
    detached.movementInBlock = false;
    Add(world, detached);
    Fixture distant = UnitAt(7, 60.0f, 0.0f);
    Add(world, distant);
    Fixture entityless = UnitAt(8, 3.0f, 3.0f);
    entityless.transportGuid = 0xF120000000005678ull;
    entityless.entity = false;
    Add(world, entityless);

    NearbyUnitFrame frame;
    const bool captured = engine::CaptureNearbyUnitsFrom(world.Manager(), centre, kRange, frame);
    const NearbyUnit* rides = UnitOf(frame, 1);
    const NearbyUnit* walks = UnitOf(frame, 2);
    const NearbyUnit* self = UnitOf(frame, 3);
    const NearbyUnit* aboard = UnitOf(frame, 4);
    Check(captured && frame.count == 4 && rides && walks && self && aboard && !UnitOf(frame, 5) && !UnitOf(frame, 6) &&
              !UnitOf(frame, 7),
          "the walk of the visible objects finds the units in range, not game objects, half-built units, units "
          "without an entity on a transport or distant ones");
    Check(rides && rides->mounted && walks && !walks->mounted && self && !self->mounted,
          "a unit is mounted when the client's own check says so: a mount display and no suppressing flag");
    Check(walks && std::fabs(walks->radius - 0.9f) < 1.0e-6f && walks->position[0] == -4.0f &&
              walks->position[1] == 3.0f && walks->position[2] == 5.0f,
          "a unit is read with its collision radius and its own position");
    Check(aboard && aboard->position[0] == 8.0f && aboard->position[1] == -2.0f && aboard->position[2] == 9.0f &&
              frame.units[0].guid == 3 && frame.units[1].guid == 2 && frame.units[2].guid == 1,
          "a unit on a transport is read at the position of its world entity, and the walk keeps them nearest first");

    NearbyUnitFrame none;
    Check(engine::CaptureNearbyUnitsFrom(0, centre, kRange, none) && none.count == 0,
          "no object manager is an empty frame, not a failure");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float badCentre[3] = {nan, 0.0f, 0.0f};
    NearbyUnitFrame refused;
    Check(!engine::CaptureNearbyUnitsFrom(world.Manager(), badCentre, kRange, refused) && refused.count == 0,
          "a centre that is not a position captures nothing");

    SyntheticObjects cut;
    cut.AddObject(water_contact_checks::kSmallObjectBytes, water_contact_checks::kItemTypeMask);
    Add(cut, UnitAt(9, 1.0f, 1.0f));
    NearbyUnitFrame cutFrame;
    Check(engine::CaptureNearbyUnitsFrom(cut.Manager(), centre, kRange, cutFrame) && cutFrame.count == 1,
          "an object that is not a unit is stepped over without ending the walk");
}

void CheckNearbyUnits()
{
    CheckSelection();
    CheckVisibleUnitWalk();
}
}

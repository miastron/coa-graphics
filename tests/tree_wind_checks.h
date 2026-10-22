#pragma once

namespace tree_wind_checks
{
constexpr size_t kInstanceBytes = 0x100;
constexpr size_t kSharedBytes = 0x1A8;
constexpr size_t kCollidingStride = 0x20000;
constexpr size_t kCollidingInstances = 40;
constexpr uint32_t kFreshTick = 100000;
constexpr uint32_t kRetiredAfterMs = 2500;
constexpr float kTolerance = 1.0e-5f;
constexpr float kTreeHeight = 12.0f;
constexpr float kPlacedAt[3] = {1200.0f, -340.0f, 55.0f};

struct FakeModel
{
    std::vector<unsigned char> instance = std::vector<unsigned char>(kInstanceBytes, 0);
    std::vector<unsigned char> shared = std::vector<unsigned char>(kSharedBytes, 0);

    float* Placement() { return reinterpret_cast<float*>(instance.data() + engine::kModelPlacement); }

    FakeModel(const char* name, uint32_t flags, float height, float scale = 1.0f)
    {
        std::memcpy(instance.data() + engine::kModelFlags, &flags, sizeof(flags));
        Link(shared.data());
        SetName(name);
        SetHeight(height);
        float* p = Placement();
        p[0] = p[5] = p[10] = scale;
        p[15] = 1.0f;
        p[12] = kPlacedAt[0];
        p[13] = kPlacedAt[1];
        p[14] = kPlacedAt[2];
    }

    void Link(unsigned char* sharedBytes)
    {
        const uintptr_t address = reinterpret_cast<uintptr_t>(sharedBytes);
        std::memcpy(instance.data() + engine::kModelShared, &address, sizeof(address));
        const uintptr_t loaded = 1;
        std::memcpy(shared.data() + engine::kSharedData, &loaded, sizeof(loaded));
    }

    void SetName(const char* name)
    {
        char* buffer = reinterpret_cast<char*>(shared.data() + engine::kSharedFileName);
        std::strncpy(buffer, name, engine::kSharedFileNameSize - 1);
        const uintptr_t stem = reinterpret_cast<uintptr_t>(buffer);
        std::memcpy(shared.data() + engine::kSharedFileStem, &stem, sizeof(stem));
    }

    void SetHeight(float height)
    {
        const float box[6] = {-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, height};
        std::memcpy(shared.data() + engine::kSharedBounds, box, sizeof(box));
    }

    void SetStem(uintptr_t stem) { std::memcpy(shared.data() + engine::kSharedFileStem, &stem, sizeof(stem)); }
    void SetLoaded(bool loaded)
    {
        const uintptr_t value = loaded ? 1 : 0;
        std::memcpy(shared.data() + engine::kSharedData, &value, sizeof(value));
    }
};

TreeWindSettings Settings()
{
    return TreeWindSettingsFrom(Config());
}

float Distance(const float* a, const float* b, size_t count)
{
    float worst = 0.0f;
    for (size_t i = 0; i < count; ++i)
        worst = std::max(worst, std::fabs(a[i] - b[i]));
    return worst;
}

float RowLength(const float* placement, int row)
{
    const float* r = placement + row * 4;
    return std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
}

bool Swayed(TreeWind& wind, FakeModel& model, uint32_t tick = kFreshTick)
{
    const float camera[3] = {kPlacedAt[0], kPlacedAt[1], kPlacedAt[2]};
    float before[engine::kPlacementFloats];
    std::memcpy(before, model.Placement(), sizeof(before));
    wind.Sway(model.instance.data(), camera, tick);
    return Distance(before, model.Placement(), engine::kPlacementFloats) > kTolerance;
}

void CheckStrengthAndPlacement()
{
    TreeWind wind;
    TreeWindSettings still = Settings();
    still.treeWave.amplitudeDeg = 0.0f;
    still.treeWave.crossAmplitudeDeg = 0.0f;
    still.treeWave.leanDeg = 0.0f;
    wind.Configure(still);
    FakeModel idle("Tree01.m2", engine::kMapDoodadModelFlag, kTreeHeight);
    float pristine[engine::kPlacementFloats];
    std::memcpy(pristine, idle.Placement(), sizeof(pristine));
    const bool moved = Swayed(wind, idle);
    Check(!moved && Distance(pristine, idle.Placement(), engine::kPlacementFloats) < kTolerance,
          "tree wind at zero strength leaves the placement matrix as it was");

    wind.Configure(Settings());
    FakeModel tree("Tree01.m2", engine::kMapDoodadModelFlag, kTreeHeight, 2.0f);
    std::memcpy(pristine, tree.Placement(), sizeof(pristine));
    const bool swayed = Swayed(wind, tree);
    const float* p = tree.Placement();
    float axisLength = 0.0f;
    for (int row = 0; row < 3; ++row)
        axisLength = std::max(axisLength, std::fabs(RowLength(p, row) - 2.0f));
    Check(swayed && wind.HasSwayed(), "a placed tree's matrix is tilted by the wind");
    Check(p[12] == kPlacedAt[0] && p[13] == kPlacedAt[1] && p[14] == kPlacedAt[2] && p[15] == 1.0f,
          "the tilt keeps the tree's translation");
    Check(axisLength < kTolerance * 10.0f, "the tilt is a rotation: the tree keeps its scale");
}

void CheckGating()
{
    TreeWind wind;
    wind.Configure(Settings());
    const uint32_t doodad = engine::kMapDoodadModelFlag;
    FakeModel live("PineTree03.m2", doodad, kTreeHeight);
    FakeModel dead("DeadTree02.m2", doodad, kTreeHeight);
    FakeModel stump("OakStump.m2", doodad, kTreeHeight);
    FakeModel rock("RockSpire01.m2", doodad, kTreeHeight);
    FakeModel unit("PineTree03.m2", 0, kTreeHeight);
    FakeModel giant("WorldTree.m2", doodad, kTreeHeight, 40.0f);
    Check(Swayed(wind, live), "a live tree name sways");
    Check(!Swayed(wind, dead) && !Swayed(wind, stump), "dead, bare and stump models stay still");
    Check(!Swayed(wind, rock), "a model without a foliage name stays still");
    Check(!Swayed(wind, unit), "a model that is not a map-placed doodad stays still");
    Check(!Swayed(wind, giant), "a tree taller than the height limit stays still");

    TreeWindSettings limited = Settings();
    limited.treeMaxDistance = 50.0f;
    wind.Configure(limited);
    FakeModel distant("PineTree03.m2", doodad, kTreeHeight);
    distant.Placement()[12] = kPlacedAt[0] + 500.0f;
    Check(!Swayed(wind, distant), "a tree beyond the distance limit stays still");

    wind.Configure(Settings());
    FakeModel unloaded("PineTree03.m2", doodad, kTreeHeight);
    unloaded.SetLoaded(false);
    Check(!Swayed(wind, unloaded), "a model that has not finished loading stays still");
    FakeModel stray("PineTree03.m2", doodad, kTreeHeight);
    stray.SetStem(reinterpret_cast<uintptr_t>(&stray) + 1);
    Check(!Swayed(wind, stray), "a model name pointing outside the model's own file name buffer is not read");

    TreeWindSettings tall = Settings();
    tall.treeMatchNames = false;
    tall.treeMatchTallThin = true;
    wind.Configure(tall);
    FakeModel spire("RockSpire01.m2", doodad, kTreeHeight);
    FakeModel boulder("Boulder.m2", doodad, 1.0f);
    Check(Swayed(wind, spire) && !Swayed(wind, boulder), "the tall-model test sways tall narrow models only");
}

void CheckBushes()
{
    TreeWind wind;
    TreeWindSettings treesOff = Settings();
    treesOff.trees = false;
    wind.Configure(treesOff);
    FakeModel fern("Fern02.m2", engine::kMapDoodadModelFlag, 1.5f);
    FakeModel tree("PineTree03.m2", engine::kMapDoodadModelFlag, kTreeHeight);
    Check(Swayed(wind, fern) && !Swayed(wind, tree), "bushes sway by name alone with the trees switched off");

    TreeWindSettings bushesOff = Settings();
    bushesOff.bushes = false;
    wind.Configure(bushesOff);
    FakeModel again("Fern02.m2", engine::kMapDoodadModelFlag, 1.5f);
    Check(!Swayed(wind, again), "bushes stay still with bush wind switched off");
}

void CheckReusedAddressReseeds(TreeWind& wind)
{
    constexpr float kReplacementScale = 2.0f;
    constexpr float kReplacementShift = 90.0f;
    FakeModel tree("PineTree03.m2", engine::kMapDoodadModelFlag, kTreeHeight);
    const float camera[3] = {kPlacedAt[0], kPlacedAt[1], kPlacedAt[2]};
    wind.Sway(tree.instance.data(), camera, kFreshTick);

    float* p = tree.Placement();
    std::memset(p, 0, engine::kPlacementFloats * sizeof(float));
    p[0] = p[5] = p[10] = kReplacementScale;
    p[12] = kPlacedAt[0] + kReplacementShift;
    wind.Sway(tree.instance.data(), camera, kFreshTick);
    Check(std::fabs(RowLength(p, 0) - kReplacementScale) < kTolerance * 10.0f,
          "a tree placed at a reused instance address with the same model is swayed from its own placement");
    wind.Clear();
}

void CheckPlacementCache()
{
    TreeWind wind;
    wind.Configure(Settings());
    FakeModel tree("PineTree03.m2", engine::kMapDoodadModelFlag, kTreeHeight);
    const float camera[3] = {kPlacedAt[0], kPlacedAt[1], kPlacedAt[2]};
    wind.Sway(tree.instance.data(), camera, kFreshTick);
    float once[engine::kPlacementFloats];
    std::memcpy(once, tree.Placement(), sizeof(once));
    for (int frame = 0; frame < 20; ++frame)
        wind.Sway(tree.instance.data(), camera, kFreshTick);
    Check(Distance(once, tree.Placement(), engine::kPlacementFloats) < kTolerance,
          "swaying the same tree again at the same time rebuilds the tilt from its pristine placement");
    Check(wind.CachedInstances() == 1, "one tree takes one cache entry");
    wind.Clear();
    Check(wind.CachedInstances() == 0, "the cache is cleared on a world change");

    CheckReusedAddressReseeds(wind);

    unsigned char* region = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, kCollidingStride * kCollidingInstances, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Check(region != nullptr, "the colliding instances were allocated");
    if (!region)
        return;
    std::vector<FakeModel> models;
    models.reserve(kCollidingInstances);
    std::vector<unsigned char*> slots;
    for (size_t i = 0; i < kCollidingInstances; ++i)
    {
        models.emplace_back("PineTree03.m2", engine::kMapDoodadModelFlag, kTreeHeight);
        unsigned char* slot = region + i * kCollidingStride;
        std::memcpy(slot, models.back().instance.data(), kInstanceBytes);
        const uintptr_t shared = reinterpret_cast<uintptr_t>(models.back().shared.data());
        std::memcpy(slot + engine::kModelShared, &shared, sizeof(shared));
        slots.push_back(slot);
    }
    unsigned swayed = 0;
    for (unsigned char* slot : slots)
    {
        float before[engine::kPlacementFloats];
        std::memcpy(before, slot + engine::kModelPlacement, sizeof(before));
        wind.Sway(slot, camera, kFreshTick);
        swayed += Distance(before, reinterpret_cast<float*>(slot + engine::kModelPlacement),
                           engine::kPlacementFloats) > kTolerance;
    }
    Check(swayed < kCollidingInstances, "a saturated neighbourhood of live trees leaves the newest tree still");
    unsigned char* newest = slots.back();
    float before[engine::kPlacementFloats];
    std::memcpy(before, newest + engine::kModelPlacement, sizeof(before));
    wind.Sway(newest, camera, kFreshTick + kRetiredAfterMs);
    Check(Distance(before, reinterpret_cast<float*>(newest + engine::kModelPlacement), engine::kPlacementFloats) >
              kTolerance,
          "an entry untouched for two seconds is reclaimed for a newly streamed tree");
    VirtualFree(region, 0, MEM_RELEASE);
}

struct AnimateCall
{
    int calls = 0;
    void* instance = nullptr;
    void* arguments[3] = {};
    uint32_t last[2] = {};
    float placementAtCall[engine::kPlacementFloats] = {};
};

AnimateCall g_animateCall;

void __fastcall FakeAnimate(void* instance, void*, void* a1, void* a2, void* a3, uint32_t a4, uint32_t a5)
{
    ++g_animateCall.calls;
    g_animateCall.instance = instance;
    g_animateCall.arguments[0] = a1;
    g_animateCall.arguments[1] = a2;
    g_animateCall.arguments[2] = a3;
    g_animateCall.last[0] = a4;
    g_animateCall.last[1] = a5;
    std::memcpy(g_animateCall.placementAtCall, static_cast<char*>(instance) + engine::kModelPlacement,
                sizeof(g_animateCall.placementAtCall));
}

using AnimateThiscall = void(__thiscall*)(void*, void*, void*, void*, uint32_t, uint32_t);

bool ThunkSwaysBeforeForwarding(const void* thunk, FakeModel& model)
{
    int stackProbe = 0;
    g_animateCall = {};
    float pristine[engine::kPlacementFloats];
    std::memcpy(pristine, model.Placement(), sizeof(pristine));
    reinterpret_cast<AnimateThiscall>(thunk)(model.instance.data(), &stackProbe, &stackProbe + 1, &stackProbe + 2, 7,
                                             9);
    return g_animateCall.calls == 1 && g_animateCall.instance == model.instance.data() &&
           g_animateCall.arguments[0] == &stackProbe && g_animateCall.arguments[1] == &stackProbe + 1 &&
           g_animateCall.arguments[2] == &stackProbe + 2 && g_animateCall.last[0] == 7 && g_animateCall.last[1] == 9 &&
           Distance(pristine, g_animateCall.placementAtCall, engine::kPlacementFloats) > kTolerance;
}

void CheckThunks()
{
    static const float camera[3] = {kPlacedAt[0], kPlacedAt[1], kPlacedAt[2]};
    const void* full = nullptr;
    const void* simple = nullptr;
    vf_test_model_animate_thunks(reinterpret_cast<uintptr_t>(&FakeAnimate), reinterpret_cast<uintptr_t>(&FakeAnimate),
                                 camera, &full, &simple);
    FakeModel fullTree("PineTree03.m2", engine::kMapDoodadModelFlag, kTreeHeight);
    FakeModel simpleTree("OakTree01.m2", engine::kMapDoodadModelFlag, kTreeHeight);
    const bool fullForwarded = ThunkSwaysBeforeForwarding(full, fullTree);
    const bool simpleForwarded = ThunkSwaysBeforeForwarding(simple, simpleTree);
    Check(fullForwarded && simpleForwarded,
          "both model animate thunks tilt the tree first, then forward the instance and the five stack arguments");
}

void CheckGuards()
{
    size_t count = 0;
    const ClientCodeGuard* guards = engine::TreeWindGuards(count);
    uintptr_t low = UINTPTR_MAX;
    uintptr_t high = 0;
    for (size_t i = 0; i < count; ++i)
    {
        low = std::min(low, guards[i].address);
        high = std::max(high, guards[i].address + guards[i].size);
    }
    std::vector<unsigned char> bytes(high - low, 0xCC);
    for (size_t i = 0; i < count; ++i)
        std::memcpy(&bytes[guards[i].address - low], guards[i].bytes, guards[i].size);
    const ClientCodeView view = {low, bytes.data(), bytes.size()};
    const bool matches = engine::TreeWindCodeMatches(view);
    bytes[guards[count - 1].address - low] ^= 0xFF;
    const bool changed = engine::TreeWindCodeMatches(view);
    Check(matches && !changed, "the tree wind hooks need every guarded byte of the client's model animate code");
}

bool SameWave(const TreeWindWave& a, const TreeWindWave& b)
{
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}

void CheckShippedSettings(const std::wstring& shippedIni)
{
    ConfigStore store;
    store.Load(NarrowPath(shippedIni));
    const TreeWindSettings shipped = TreeWindSettingsFrom(store.Get());
    const TreeWindSettings defaults = Settings();
    Check(shipped.trees == defaults.trees && shipped.bushes == defaults.bushes &&
              SameWave(shipped.treeWave, defaults.treeWave) && SameWave(shipped.bushWave, defaults.bushWave) &&
              shipped.treeDoodadsOnly == defaults.treeDoodadsOnly &&
              shipped.treeMatchNames == defaults.treeMatchNames &&
              shipped.treeMatchTallThin == defaults.treeMatchTallThin &&
              shipped.treeSkipDead == defaults.treeSkipDead &&
              shipped.treeMinHeight == defaults.treeMinHeight && shipped.treeMinAspect == defaults.treeMinAspect &&
              shipped.treeMaxHeight == defaults.treeMaxHeight &&
              shipped.treeMaxDistance == defaults.treeMaxDistance &&
              shipped.bushDoodadsOnly == defaults.bushDoodadsOnly && shipped.bushSkipDead == defaults.bushSkipDead &&
              shipped.bushMaxDistance == defaults.bushMaxDistance,
          "the shipped INI carries the tree and bush wind defaults");
}

void CheckTreeWind()
{
    CheckStrengthAndPlacement();
    CheckGating();
    CheckBushes();
    CheckPlacementCache();
    CheckGuards();
    CheckThunks();
}
}

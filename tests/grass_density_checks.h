#pragma once

namespace grass_density_checks
{
constexpr float kTolerance = 1.0e-3f;
constexpr float kHalfChunk = kGrassChunkSize * 0.5f;

void WorldAt(float (&matrix)[16], float x, float y)
{
    std::memset(matrix, 0, sizeof(matrix));
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
    matrix[kWorldMatrixTranslationX] = x;
    matrix[kWorldMatrixTranslationY] = y;
}

float DistanceOfChunkAt(float x, float y)
{
    float matrix[16];
    WorldAt(matrix, x, y);
    return GrassChunkDistance(matrix);
}

void CheckChunkDistance()
{
    const float inside = DistanceOfChunkAt(kHalfChunk, kHalfChunk);
    const float cornerInside = DistanceOfChunkAt(1.0f, 1.0f);
    const float east = DistanceOfChunkAt(-50.0f, kHalfChunk);
    const float north = DistanceOfChunkAt(kHalfChunk, kGrassChunkSize + 20.0f);
    const float diagonal = DistanceOfChunkAt(-30.0f, kGrassChunkSize + 40.0f);
    Check(inside == 0.0f && cornerInside == 0.0f,
          "a chunk the camera stands in is at distance 0, whichever of its corners the world matrix names");
    Check(std::fabs(east - 50.0f) < kTolerance && std::fabs(north - 20.0f) < kTolerance,
          "the distance of a chunk is that of its nearest edge, the chunk reaching from its top left back by 33.33 yd");
    Check(std::fabs(diagonal - 50.0f) < kTolerance,
          "a chunk diagonal to the camera is at the distance of its nearest corner");
}

void CheckExtraCopies()
{
    const GrassDensitySettings stock = {1.0f, 60.0f, 1.0f};
    const GrassDensitySettings doubled = {2.0f, 60.0f, 1.0f};
    const GrassDensitySettings tripled = {3.0f, 60.0f, 1.0f};
    const GrassDensitySettings quadrupled = {4.0f, 60.0f, 1.0f};
    const GrassDensitySettings huge = {40.0f, 60.0f, 1.0f};
    const GrassDensitySettings noRadius = {3.0f, 0.0f, 1.0f};
    Check(GrassExtraCopies(stock, 0.0f) == 0 && GrassExtraCopies(noRadius, 0.0f) == 0 && GrassCopyCount(stock) == 0 &&
              GrassCopyCount(noRadius) == 0,
          "a multiplier of 1 or a radius of 0 gives no extra copies anywhere");
    Check(GrassCopyCount(doubled) == 1 && GrassCopyCount(tripled) == 2 && GrassCopyCount(quadrupled) == 3 &&
              GrassCopyCount(huge) == kGrassMaxExtraCopies && GrassCopyCount({2.5f, 60.0f, 1.0f}) == 2,
          "a plant has as many copies as the multiplier says, rounded up, at most the shader's three");
    Check(GrassExtraCopies(doubled, 0.0f) == 1 && GrassExtraCopies(doubled, 59.0f) == 1 &&
              GrassExtraCopies(doubled, 60.0f) == 0 && GrassExtraCopies(doubled, 500.0f) == 0,
          "a multiplier of 2 draws its copy for every chunk inside the radius and none beyond it");
    Check(GrassExtraCopies(quadrupled, 0.0f) == 3 && GrassExtraCopies(quadrupled, 39.0f) == 3 &&
              GrassExtraCopies(quadrupled, 41.0f) == 2 && GrassExtraCopies(quadrupled, 51.0f) == 1 &&
              GrassExtraCopies(quadrupled, 61.0f) == 0,
          "a multiplier of 4 draws the third copy out to 40 yd, the second to 50 and the first to 60");
    unsigned previous = kGrassMaxExtraCopies;
    bool never = true;
    for (float distance = 0.0f; distance <= 80.0f; distance += 0.5f)
    {
        const unsigned copies = GrassExtraCopies(quadrupled, distance);
        never = never && copies <= previous;
        previous = copies;
    }
    Check(never, "the copies drawn never grow with distance");

    const GrassCopyWindow first = GrassCopyWindowFor(tripled, 1);
    const GrassCopyWindow second = GrassCopyWindowFor(tripled, 2);
    const GrassCopyWindow partial = GrassCopyWindowFor({2.5f, 60.0f, 1.0f}, 2);
    Check(std::fabs(first.outerDistance - 60.0f) < kTolerance && std::fabs(first.width - 15.0f) < kTolerance &&
              first.inverseShare == 1.0f && std::fabs(second.outerDistance - 45.0f) < kTolerance &&
              std::fabs(second.width - 15.0f) < kTolerance && second.inverseShare == 1.0f &&
              std::fabs(partial.outerDistance - 40.0f) < kTolerance && std::fabs(partial.width - 10.0f) < kTolerance &&
              std::fabs(partial.inverseShare - 2.0f) < kTolerance,
          "each copy thins out over its own band of the distance between half the radius and the radius, the last "
          "copy of a fractional multiplier over a shorter band and for half of the plants");
}

float VisibleShare(const GrassCopyWindow& window, float distance)
{
    constexpr float kSunkWhenVisible = 1.0f;
    constexpr unsigned kPatterns = 1u << kGrassPlantBitCount;
    unsigned visible = 0;
    for (unsigned bits = 0; bits < kPatterns; ++bits)
        visible += GrassPlantSink(window, bits, distance) < kSunkWhenVisible ? 1u : 0u;
    return static_cast<float>(visible) / kPatterns;
}

void CheckSink()
{
    const GrassDensitySettings tripled = {3.0f, 60.0f, 1.0f};
    const GrassDensitySettings fractional = {2.5f, 60.0f, 1.0f};
    bool steady = true;
    bool clean = true;
    for (unsigned copy = 1; copy <= 2; ++copy)
    {
        const GrassCopyWindow window = GrassCopyWindowFor(tripled, copy);
        float previous = 1.0f;
        for (float distance = 0.0f; distance <= 80.0f; distance += 1.0f)
        {
            const float share = VisibleShare(window, distance);
            steady = steady && share <= previous + kTolerance;
            previous = share;
        }
        const float beyond = window.outerDistance + 0.5f;
        clean = clean && VisibleShare(window, 5.0f) > 0.99f && VisibleShare(window, beyond) == 0.0f;
    }
    const GrassCopyWindow outer = GrassCopyWindowFor(tripled, 1);
    const GrassCopyWindow inner = GrassCopyWindowFor(tripled, 2);
    const float halfSunk = 0.5f / outer.inverseFade;
    const float halfway = VisibleShare(outer, outer.outerDistance - 0.5f * outer.width - halfSunk);
    const float innerHalfway = VisibleShare(inner, inner.outerDistance - 0.5f * inner.width - halfSunk);
    const GrassCopyWindow lastOfThree = GrassCopyWindowFor(fractional, 2);
    Check(steady && clean,
          "the share of plants that still show a copy only falls with distance, is everyone near the camera and none "
          "past the copy's outer distance");
    Check(halfway > 0.3f && halfway < 0.7f && innerHalfway > 0.3f && innerHalfway < 0.7f,
          "about half of the plants show a copy in the middle of its band");
    Check(VisibleShare(lastOfThree, 0.0f) > 0.4f && VisibleShare(lastOfThree, 0.0f) < 0.6f,
          "of a fractional multiplier's last copy only the matching share of the plants ever shows it");
    Check(GrassPlantSink(GrassCopyWindow{0.0f, 0.0f, 0.0f}, 17, 30.0f) == 0.0f,
          "a draw without a copy window sinks nothing");
    float deepest = 0.0f;
    for (unsigned bits = 0; bits < (1u << kGrassPlantBitCount); ++bits)
        deepest = std::max(deepest, GrassPlantSink(outer, bits, 100.0f));
    Check(std::fabs(deepest - kGrassSinkDepth) < kTolerance,
          "a plant that is out of range sinks by the full depth into the ground");
}

void CheckOffsets()
{
    const GrassDensitySettings narrow = {2.0f, 60.0f, 1.0f};
    const GrassDensitySettings wide = {2.0f, 60.0f, 2.0f};
    bool inRange = true;
    bool scales = true;
    float angles[kGrassMaxExtraCopies + 1] = {};
    for (unsigned copy = 1; copy <= kGrassMaxExtraCopies; ++copy)
    {
        const GrassCopyOffset a = GrassCopyOffsetFor(narrow, copy);
        const GrassCopyOffset b = GrassCopyOffsetFor(wide, copy);
        const float length = std::sqrt(a.x * a.x + a.y * a.y);
        inRange = inRange && length >= 0.5f - kTolerance && length <= 1.0f + kTolerance;
        scales = scales && std::fabs(b.x - 2.0f * a.x) < kTolerance && std::fabs(b.y - 2.0f * a.y) < kTolerance;
        angles[copy] = std::atan2(a.y, a.x);
    }
    bool distinct = true;
    for (unsigned a = 1; a <= kGrassMaxExtraCopies; ++a)
        for (unsigned b = a + 1; b <= kGrassMaxExtraCopies; ++b)
        {
            float gap = std::fabs(angles[a] - angles[b]);
            gap = std::min(gap, 6.2831853f - gap);
            distinct = distinct && gap > 0.5f;
        }
    const GrassCopyOffset again = GrassCopyOffsetFor(narrow, 2);
    const GrassCopyOffset once = GrassCopyOffsetFor(narrow, 2);
    Check(inRange && scales && distinct && again.x == once.x && again.y == once.y,
          "each copy lies between half the spread and the spread from its plant, scales with the spread, differs in "
          "direction from the other copies and is the same every time");
}

void CheckPlantBits()
{
    constexpr unsigned kBuckets = 1u << kGrassPlantBitCount;
    constexpr unsigned kPlants = 20000;
    unsigned counts[kBuckets] = {};
    unsigned seed = 12345;
    bool stable = true;
    for (unsigned i = 0; i < kPlants; ++i)
    {
        seed = seed * 1664525u + 1013904223u;
        const float x = static_cast<float>(seed >> 8) * 0.0005f;
        seed = seed * 1664525u + 1013904223u;
        const float y = static_cast<float>(seed >> 8) * 0.0005f;
        const unsigned bits = GrassPlantBits(x, y);
        stable = stable && bits < kBuckets && bits == GrassPlantBits(x, y);
        ++counts[bits < kBuckets ? bits : 0];
    }
    const float expected = static_cast<float>(kPlants) / kBuckets;
    bool even = true;
    for (unsigned bucket = 0; bucket < kBuckets; ++bucket)
        even = even && counts[bucket] > expected * 0.7f && counts[bucket] < expected * 1.3f;
    const unsigned nearby = GrassPlantBits(100.25f, 200.5f);
    unsigned different = 0;
    for (int step = 1; step <= 8; ++step)
        different += GrassPlantBits(100.25f + 0.01f * static_cast<float>(step), 200.5f) != nearby ? 1u : 0u;
    Check(stable && even && different >= 6,
          "a plant's six random bits come from its position, are the same each time, spread evenly over the 64 values "
          "and differ between plants a hair apart");

    const GrassCopyOffset copy = {0.8f, 0.0f};
    bool lengths = true;
    float angleSum = 0.0f;
    for (unsigned bits = 0; bits < kBuckets; ++bits)
    {
        const GrassCopyOffset moved = GrassPlantOffset(copy, bits);
        const float length = std::sqrt(moved.x * moved.x + moved.y * moved.y);
        lengths = lengths && length >= 0.8f * kGrassPlantMagnitudeFloor - kTolerance && length <= 0.8f + kTolerance;
        angleSum += std::atan2(moved.y, moved.x);
    }
    const GrassCopyOffset first = GrassPlantOffset(copy, 0);
    const GrassCopyOffset last = GrassPlantOffset(copy, kBuckets - 1);
    const float turn = std::fabs(std::atan2(first.y, first.x) - std::atan2(last.y, last.x));
    Check(lengths && turn > 5.0f && std::fabs(angleSum) < 8.0f,
          "a plant turns its copy's offset by an angle spread evenly over a full turn and shortens it to between "
          "0.6 and 1 of its length");
}

void CheckSettings(const std::wstring& outDir, const std::wstring& shippedIni)
{
    const Config defaults;
    const GrassDensitySettings fromDefaults = GrassDensitySettingsFrom(defaults);
    ConfigStore shipped;
    shipped.Load(NarrowPath(shippedIni));
    const GrassDensitySettings fromShipped = GrassDensitySettingsFrom(shipped.Get());
    Check(fromDefaults.multiplier == 2.0f && fromDefaults.radius == 150.0f && fromDefaults.spread == 1.0f &&
              fromDefaults.fade == 32.0f && fromShipped.fade == fromDefaults.fade &&
              fromShipped.multiplier == fromDefaults.multiplier && fromShipped.radius == fromDefaults.radius &&
              fromShipped.spread == fromDefaults.spread,
          "the shipped INI and the built-in defaults draw every plant twice within 150 yd, a yard from its plant");

    const std::wstring edited = FullPath(outDir + L"\\grass-density.ini");
    CopyFileW(shippedIni.c_str(), edited.c_str(), FALSE);
    float read[3][4] = {};
    const wchar_t* values[3][4] = {{L"0", L"1", L"0", L"0"}, {L"3", L"100", L"2.5", L"12"},
                                   {L"9", L"900", L"9", L"99"}};
    const wchar_t* keys[4] = {L"GrassDensity", L"GrassDensityRadius", L"GrassDensitySpread",
                              L"GrassDensityFade"};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 4; ++column)
            WritePrivateProfileStringW(L"CoAGraphics", keys[column], values[row][column], edited.c_str());
        ConfigStore store;
        store.Load(NarrowPath(edited));
        read[row][0] = store.Get().grassDensity;
        read[row][1] = store.Get().grassDensityRadius;
        read[row][2] = store.Get().grassDensitySpread;
        read[row][3] = store.Get().grassDensityFade;
    }
    Check(read[0][0] == 1.0f && read[0][1] == 10.0f && read[0][2] == 0.25f && read[0][3] == 1.0f &&
              read[1][0] == 3.0f && read[1][1] == 100.0f && read[1][2] == 2.5f && read[1][3] == 12.0f &&
              read[2][0] == 4.0f && read[2][1] == 250.0f && read[2][2] == 3.0f && read[2][3] == 40.0f,
          "GrassDensity, GrassDensityRadius, GrassDensitySpread and GrassDensityFade are read from the INI and "
          "held to 1 to 4, 10 to 250 yd, 0.25 to 3 yd and 1 to 40 yd");
}

struct DrawLog
{
    int draws = 0;
    std::vector<float> offsetsAtDraw;
    std::vector<void*> slots;
};

DrawLog g_drawLog;
float g_currentOffset[2] = {};
unsigned g_offsetUploads = 0;

void __cdecl FakeInitializeConstants()
{
}

void __cdecl FakeSetup(const float*, int)
{
}

void __fastcall FakeDraw(void* slot, void*)
{
    ++g_drawLog.draws;
    g_drawLog.slots.push_back(slot);
    g_drawLog.offsetsAtDraw.push_back(g_currentOffset[0]);
    g_drawLog.offsetsAtDraw.push_back(g_currentOffset[1]);
}

void __cdecl FakePassEnd()
{
}

bool NoBands(uint32_t, ModelBands&)
{
    return false;
}

void RecordUpload(unsigned first, const float* data, unsigned count)
{
    if (first == kGrassCopyRegister && count == kGrassCopyRegisterCount)
    {
        ++g_offsetUploads;
        g_currentOffset[0] = data[0];
        g_currentOffset[1] = data[1];
    }
}

void ResetLog()
{
    g_drawLog = {};
    g_currentOffset[0] = g_currentOffset[1] = 0.0f;
    g_offsetUploads = 0;
}

using SetupFn = void(__cdecl*)(const float*, int);
using DrawFn = void(__fastcall*)(void*);

void CheckThunks()
{
    float block[grass_wind_checks::kUploadedFloats] = {};
    const float camera[3] = {};
    const GrassInputs inputs = grass_wind_checks::InputsAt(camera, camera, nullptr);
    const void* setupThunk = nullptr;
    const void* constantsThunk = vf_test_grass_thunks(reinterpret_cast<uintptr_t>(&FakeInitializeConstants),
                                                      reinterpret_cast<uintptr_t>(&FakeSetup),
                                                      block + engine::kDetailDoodadFirstFreeRegister * 4, &inputs,
                                                      &RecordUpload, &setupThunk);
    const void* drawThunk = nullptr;
    const void* passEndThunk = nullptr;
    vf_test_grass_doodad_thunks(0, reinterpret_cast<uintptr_t>(&FakeDraw), reinterpret_cast<uintptr_t>(&FakePassEnd),
                                &NoBands, nullptr, 1, &drawThunk, &passEndThunk);
    reinterpret_cast<void(__cdecl*)()>(constantsThunk)();
    vf_test_set_grass_density(3.0f, 60.0f, 1.5f);
    int slotBytes = 0;
    void* const slot = &slotBytes;
    float onTop[16];
    float remote[16];
    WorldAt(onTop, kHalfChunk, kHalfChunk);
    WorldAt(remote, -400.0f, -400.0f);

    ResetLog();
    reinterpret_cast<SetupFn>(setupThunk)(onTop, 0);
    reinterpret_cast<DrawFn>(drawThunk)(slot);
    const GrassDensitySettings settings = {3.0f, 60.0f, 1.5f};
    const GrassCopyOffset first = GrassCopyOffsetFor(settings, 1);
    const GrassCopyOffset second = GrassCopyOffsetFor(settings, 2);
    bool offsetsMatch = g_drawLog.offsetsAtDraw.size() == 6 && g_drawLog.offsetsAtDraw[0] == 0.0f &&
                        g_drawLog.offsetsAtDraw[1] == 0.0f;
    if (offsetsMatch)
    {
        const float* seen = g_drawLog.offsetsAtDraw.data();
        offsetsMatch = std::fabs(seen[2] - second.x) < kTolerance && std::fabs(seen[3] - second.y) < kTolerance &&
                       std::fabs(seen[4] - first.x) < kTolerance && std::fabs(seen[5] - first.y) < kTolerance;
    }
    bool sameSlot = true;
    for (void* seen : g_drawLog.slots)
        sameSlot = sameSlot && seen == slot;
    Check(g_drawLog.draws == 3 && offsetsMatch && sameSlot && g_offsetUploads == 3 && g_currentOffset[0] == 0.0f &&
              g_currentOffset[1] == 0.0f,
          "a chunk near the camera is drawn once for the plants and once for each copy with the slot, each copy's "
          "offset in c20 at its draw, and c20 back to zero after");

    ResetLog();
    reinterpret_cast<SetupFn>(setupThunk)(remote, 0);
    reinterpret_cast<DrawFn>(drawThunk)(slot);
    Check(g_drawLog.draws == 1 && g_offsetUploads == 0,
          "a chunk beyond the radius is drawn once and nothing is written to c20");

    ResetLog();
    reinterpret_cast<SetupFn>(setupThunk)(onTop, 1);
    reinterpret_cast<DrawFn>(drawThunk)(slot);
    Check(g_drawLog.draws == 1 && g_offsetUploads == 0,
          "the point light variant of the grass shader is never drawn twice");

    vf_test_set_grass_density(1.0f, 60.0f, 1.5f);
    ResetLog();
    reinterpret_cast<SetupFn>(setupThunk)(onTop, 0);
    reinterpret_cast<DrawFn>(drawThunk)(slot);
    Check(g_drawLog.draws == 1 && g_offsetUploads == 0, "a multiplier of 1 draws every chunk once");

    vf_test_set_grass_density(2.0f, 60.0f, 1.5f);
    ResetLog();
    reinterpret_cast<SetupFn>(setupThunk)(onTop, 0);
    reinterpret_cast<DrawFn>(drawThunk)(slot);
    reinterpret_cast<SetupFn>(setupThunk)(remote, 0);
    reinterpret_cast<DrawFn>(drawThunk)(slot);
    reinterpret_cast<SetupFn>(setupThunk)(onTop, 0);
    reinterpret_cast<DrawFn>(drawThunk)(slot);
    reinterpret_cast<DrawFn>(drawThunk)(slot);
    Check(g_drawLog.draws == 7 && g_offsetUploads == 6 && g_currentOffset[0] == 0.0f && g_currentOffset[1] == 0.0f,
          "every batch of a chunk gets its copies and a chunk farther out between two near ones does not inherit them");
    vf_test_set_grass_density(2.0f, 60.0f, 1.0f);
}

void CheckGrassDensity(const std::wstring& outDir, const std::wstring& shippedIni)
{
    CheckChunkDistance();
    CheckExtraCopies();
    CheckSink();
    CheckOffsets();
    CheckPlantBits();
    CheckSettings(outDir, shippedIni);
    CheckThunks();
}
}

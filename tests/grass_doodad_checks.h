#pragma once

namespace grass_doodad_checks
{
constexpr float kTolerance = 1.0e-5f;
constexpr size_t kShippedOverrides = 37;
constexpr uint32_t kStiffIgnoredId = 19;
constexpr uint32_t kStiffPlantId = 277;
constexpr uint32_t kBladeId = 5;
constexpr uint32_t kFlatId = 6;
constexpr uint32_t kBrokenId = 7;
constexpr uint32_t kGroundGrassId = 8;
constexpr size_t kSlotBytes = 0x40;
constexpr size_t kInstanceBytes = 0x2C;
constexpr uint32_t kBaseColour = 0xFF806040u;

template <typename T>
void Put(uintptr_t address, T value)
{
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
}

template <typename T>
T Get(uintptr_t address)
{
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    return value;
}

class NoAccessPage
{
public:
    NoAccessPage() : m_page(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE, PAGE_NOACCESS)) {}
    ~NoAccessPage() { VirtualFree(m_page, 0, MEM_RELEASE); }
    uintptr_t Address() const { return reinterpret_cast<uintptr_t>(m_page); }

private:
    void* m_page;
};

class Arena
{
public:
    uintptr_t Allocate(size_t bytes)
    {
        m_blocks.emplace_back((bytes + sizeof(uint32_t) - 1) / sizeof(uint32_t) + 1, 0u);
        return reinterpret_cast<uintptr_t>(m_blocks.back().data());
    }

private:
    std::vector<std::vector<uint32_t>> m_blocks;
};

std::vector<ModelVertex> Blade(float height, float rootV, float tipV, size_t rows = 6)
{
    std::vector<ModelVertex> vertices;
    for (size_t i = 0; i < rows; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(rows - 1);
        vertices.push_back({height * t, rootV + (tipV - rootV) * t});
        vertices.push_back({height * t, rootV + (tipV - rootV) * t});
    }
    return vertices;
}

uintptr_t AddModel(Arena& arena, const std::vector<ModelVertex>& vertices, bool loaded)
{
    const uintptr_t array = arena.Allocate(vertices.size() * 48);
    const uintptr_t lookup = arena.Allocate(vertices.size() * sizeof(uint16_t));
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        Put<float>(array + i * 48 + 8, vertices[i].height);
        Put<float>(array + i * 48 + 0x24, vertices[i].v);
        Put<uint16_t>(lookup + i * sizeof(uint16_t), static_cast<uint16_t>(i));
    }
    const uintptr_t skin = arena.Allocate(0x10);
    Put<uint32_t>(skin + 4, static_cast<uint32_t>(vertices.size()));
    Put<uintptr_t>(skin + 8, lookup);
    const uintptr_t header = arena.Allocate(0x48);
    Put<uint32_t>(header + 0x3C, static_cast<uint32_t>(vertices.size()));
    Put<uintptr_t>(header + 0x40, array);
    const uintptr_t shared = arena.Allocate(0x180);
    Put<uintptr_t>(shared + 0x150, header);
    Put<uintptr_t>(shared + 0x170, skin);
    const uintptr_t model = arena.Allocate(0x40);
    Put<uint8_t>(model + 0x10, loaded ? 1 : 0);
    Put<uintptr_t>(model + 0x2C, shared);
    return model;
}

struct ClientDoodads
{
    Arena arena;
    uintptr_t listCell = 0;
    uintptr_t sizeCell = 0;

    explicit ClientDoodads(uint32_t entries)
    {
        listCell = arena.Allocate(sizeof(uintptr_t));
        sizeCell = arena.Allocate(sizeof(uint32_t));
        Put<uintptr_t>(listCell, arena.Allocate(entries * sizeof(uintptr_t)));
        Put<uint32_t>(sizeCell, entries);
    }

    void Add(uint32_t id, const std::vector<ModelVertex>& vertices, bool loaded = true)
    {
        const uintptr_t entry = arena.Allocate(8);
        Put<uintptr_t>(entry + 4, AddModel(arena, vertices, loaded));
        Put<uintptr_t>(Get<uintptr_t>(listCell) + id * sizeof(uintptr_t), entry);
    }
};

float DecodedEntry(uint32_t colour)
{
    const float green = static_cast<float>((colour >> 8) & 0xFF) / 255.0f;
    const float red = static_cast<float>((colour >> 16) & 0xFF) / 255.0f;
    float low = green * 255.0f + 0.5f;
    low = low * 0.125f;
    low = low - std::floor(low);
    low = low * 8.0f - 0.5f;
    float high = red * 255.0f + 0.5f;
    high = high * 0.25f;
    high = high - std::floor(high);
    high = high * 4.0f - 0.5f;
    return high * 8.0f + low;
}

void CheckTags()
{
    bool maskKept = true;
    bool decoded = true;
    bool distinct = true;
    for (unsigned index = 0; index < kGrassDoodadEntries; ++index)
    {
        const uint32_t tag = GrassEntryTag(index);
        maskKept = maskKept && (tag & ~kGrassTagMask) == 0;
        for (uint32_t base : {0x00000000u, 0xFFFFFFFFu, kBaseColour, 0x80FF7F3Cu})
        {
            const uint32_t tagged = (base & ~kGrassTagMask) | tag;
            const uint32_t swapped = (tagged & 0xFF00FF00u) | ((tagged & 0xFF) << 16) | ((tagged >> 16) & 0xFF);
            decoded = decoded && std::fabs(DecodedEntry(tagged) - static_cast<float>(index)) < 0.01f &&
                      std::fabs(DecodedEntry(swapped) - static_cast<float>(index)) < 0.01f;
        }
        for (unsigned other = index + 1; other < kGrassDoodadEntries; ++other)
            distinct = distinct && GrassEntryTag(other) != tag;
    }
    Check(maskKept && distinct,
          "an entry tag only uses the low three bits of green and the low two of red and blue, and no two tags agree");
    Check(decoded,
          "the shader's float decode of the vertex colour gives back the entry index for any other colour bits, with "
          "red and blue swapped or not");
    Check(DecodedEntry(0xFFFFFFFFu) == static_cast<float>(kGrassFallbackEntry),
          "a white vertex colour decodes to the fallback entry");
}

void CheckSlotEntries()
{
    GrassSlotDoodads slot = {};
    bool overflow = false;
    const unsigned first = GrassEntryFor(slot, 40, overflow);
    const unsigned second = GrassEntryFor(slot, 41, overflow);
    const unsigned again = GrassEntryFor(slot, 40, overflow);
    Check(first == 0 && second == 1 && again == 0 && slot.count == 2 && slot.ids[1] == 41 && !overflow,
          "the entries of a slot follow the order its doodads first appear, and a doodad keeps its entry");
    for (uint32_t id = 100; slot.count < kGrassSlotEntries; ++id)
        GrassEntryFor(slot, id, overflow);
    const unsigned beyond = GrassEntryFor(slot, 9999, overflow);
    Check(slot.count == kGrassSlotEntries && beyond == kGrassFallbackEntry && overflow,
          "a slot with more doodads than entries sends the extra ones to the fallback entry and reports it");
}

void CheckBands()
{
    const std::vector<ModelVertex> blade = Blade(1.0f, 0.9f, 0.1f);
    const ModelBands bands = GrassModelBands(blade.data(), blade.size());
    Check(bands.usable && std::fabs(bands.height - 1.0f) < kTolerance && std::fabs(bands.rootV - 0.9f) < kTolerance &&
              std::fabs(bands.tipV - 0.1f) < kTolerance,
          "a model's root and tip are the texture rows of its lowest and highest vertices");
    const std::vector<ModelVertex> upside = Blade(2.0f, 0.2f, 0.8f, 9);
    const ModelBands upsideBands = GrassModelBands(upside.data(), upside.size());
    Check(upsideBands.usable && std::fabs(upsideBands.rootV - 0.2f) < kTolerance &&
              std::fabs(upsideBands.tipV - 0.8f) < kTolerance && std::fabs(upsideBands.height - 2.0f) < kTolerance,
          "a model whose texture runs the other way round gives a root row below its tip row");
    const std::vector<ModelVertex> deep = {{0.0f, 0.5f}, {0.05f, 0.4f}, {1.0f, 0.1f}, {0.95f, 0.2f}, {0.5f, 0.9f}};
    const ModelBands deepBands = GrassModelBands(deep.data(), deep.size());
    Check(std::fabs(deepBands.rootV - 0.45f) < kTolerance && std::fabs(deepBands.tipV - 0.15f) < kTolerance,
          "only the lowest and highest 12 percent of a model's height count as root and tip, averaged");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const ModelVertex broken[] = {{nan, 0.5f}, {nan, 0.1f}};
    Check(!GrassModelBands(nullptr, 3).usable && !GrassModelBands(blade.data(), 0).usable &&
              !GrassModelBands(broken, 2).usable,
          "no vertices, or heights that are not numbers, give no usable root and tip");
}

void Learn(GrassDoodads& doodads, uint32_t id, float height, float rootV, float tipV)
{
    const std::vector<ModelVertex> vertices = Blade(height, rootV, tipV);
    doodads.Learn(id, GrassModelBands(vertices.data(), vertices.size()));
}

bool Entry(const GrassDoodads& doodads, uint32_t id, float rootV, float inverse, float wind, float push)
{
    float entry[4];
    doodads.Entry(id, entry);
    return std::fabs(entry[0] - rootV) < kTolerance && std::fabs(entry[1] - inverse) < kTolerance &&
           std::fabs(entry[2] - wind) < kTolerance && std::fabs(entry[3] - push) < kTolerance;
}

void CheckEntries()
{
    GrassDoodads doodads;
    Learn(doodads, kBladeId, 1.0f, 0.9f, 0.1f);
    Learn(doodads, kFlatId, 0.05f, 0.5f, 0.1f);
    const ModelVertex unreadable[] = {{0.0f, 0.5f}, {1.0f, 0.5f}};
    doodads.Learn(kBrokenId, GrassModelBands(unreadable, 2));
    Check(Entry(doodads, 9000, 1.0f, -1.0f, 1.0f, 1.0f) && Entry(doodads, kBrokenId, 1.0f, -1.0f, 1.0f, 1.0f),
          "a doodad nobody has measured and one whose root and tip rows agree bend by the old rule: root at row 1, "
          "tip at row 0");
    Check(Entry(doodads, kBladeId, 0.9f, 1.0f / (0.1f - 0.9f), 1.0f, 1.0f),
          "a measured doodad hands the shader its root row and one over its span");
    Check(Entry(doodads, kFlatId, 0.5f, 1.0f / (0.1f - 0.5f), 0.0f, 0.0f),
          "a flat doodad such as a pebble gets neither wind nor push");

    GrassOverrides overrides;
    overrides[kBladeId] = {kGrassNoWind, 0.0f, -1.0f, -1.0f};
    overrides[kFlatId] = {0, 0.5f, -1.0f, -1.0f};
    overrides[kBrokenId] = {0, 0.25f, 0.8f, 0.2f};
    overrides[kGroundGrassId] = {kGrassFlip | kGrassNoPush, 0.0f, -1.0f, -1.0f};
    doodads.SetOverrides(overrides);
    Check(Entry(doodads, kBladeId, 0.9f, 1.0f / (0.1f - 0.9f), 0.0f, 1.0f),
          "an override that switches the wind off leaves the push, and an override replaces nothing else");
    Check(Entry(doodads, kFlatId, 0.5f, 1.0f / (0.1f - 0.5f), 0.5f, 0.5f),
          "an override on a flat doodad switches it on, with its stiffness shortening both motions");
    Check(Entry(doodads, kBrokenId, 0.8f, 1.0f / (0.2f - 0.8f), 0.75f, 0.75f),
          "a doodad that could not be measured takes the root and tip of its override");
    Learn(doodads, kGroundGrassId, 1.0f, 0.9f, 0.1f);
    Check(Entry(doodads, kGroundGrassId, 0.1f, 1.0f / (0.9f - 0.1f), 1.0f, 0.0f),
          "a flipped doodad swaps its root and tip, and an override that learns of a doodad later still applies");

    GrassOverrides rigid;
    rigid[kBladeId] = {0, 2.0f, -1.0f, -1.0f};
    doodads.SetOverrides(rigid);
    Check(Entry(doodads, kBladeId, 0.9f, 1.0f / (0.1f - 0.9f), 0.0f, 0.0f) &&
              Entry(doodads, kFlatId, 0.5f, 1.0f / (0.1f - 0.5f), 0.0f, 0.0f),
          "a stiffness above 1 is held at 1, and replacing the overrides restores the automatic values of the rest");
    Check(doodads.OverrideCount() == 1 && doodads.Known() == 4 && doodads.Analyzed(kBladeId) && !doodads.Analyzed(9000),
          "the table counts its overrides and the doodads it has measured");
}

void CheckTables()
{
    GrassDoodads doodads;
    Learn(doodads, kBladeId, 1.0f, 0.9f, 0.1f);
    Learn(doodads, kFlatId, 0.05f, 0.5f, 0.1f);
    GrassSlotDoodads slot = {};
    slot.ids[0] = kFlatId;
    slot.ids[1] = kBladeId;
    slot.ids[2] = 4242;
    slot.count = 3;
    float table[kGrassDoodadEntries][4];
    doodads.BuildTable(&slot, table);
    bool fallbackRest = true;
    for (unsigned k = 3; k < kGrassDoodadEntries; ++k)
        fallbackRest = fallbackRest && table[k][0] == 1.0f && table[k][1] == -1.0f && table[k][2] == 1.0f &&
                       table[k][3] == 1.0f;
    Check(table[0][2] == 0.0f && std::fabs(table[1][0] - 0.9f) < kTolerance && table[2][0] == 1.0f && fallbackRest,
          "a slot's table has one entry for each of its doodads in order and the fallback in every other entry");
    float empty[kGrassDoodadEntries][4];
    doodads.BuildTable(nullptr, empty);
    bool allFallback = true;
    for (unsigned k = 0; k < kGrassDoodadEntries; ++k)
        allFallback = allFallback && empty[k][0] == 1.0f && empty[k][1] == -1.0f && empty[k][2] == 1.0f;
    Check(allFallback, "a slot nobody knows gets a table of fallback entries, never zeros");

    const int first = 0;
    const int second = 0;
    doodads.RecordSlot(&first, slot);
    const GrassSlotDoodads* recorded = doodads.SlotDoodads(&first);
    Check(recorded && recorded->count == 3 && recorded->ids[1] == kBladeId && !doodads.SlotDoodads(&second),
          "a slot's doodads are remembered by the slot");
    doodads.ForgetSlot(&first);
    Check(!doodads.SlotDoodads(&first), "a slot that is filled without tags is forgotten");
    doodads.RecordSlot(&first, slot);
    doodads.Clear();
    Check(!doodads.SlotDoodads(&first) && doodads.Known() == 0, "clearing the table forgets its doodads and slots");
}

void CheckOverrideFile()
{
    GrassDoodads shipped;
    std::string error;
    const bool loaded = shipped.LoadOverrides(std::string(WIND_DATA_DIR) + "\\GroundEffectDoodadWind.cdbc", error);
    if (!loaded)
        std::printf("     %s\n", error.c_str());
    Learn(shipped, kStiffIgnoredId, 1.0f, 0.9f, 0.1f);
    Learn(shipped, kStiffPlantId, 1.0f, 0.9f, 0.1f);
    float ignored[4];
    float stiff[4];
    shipped.Entry(kStiffIgnoredId, ignored);
    shipped.Entry(kStiffPlantId, stiff);
    Check(loaded && shipped.OverrideCount() == kShippedOverrides && ignored[2] == 0.0f && ignored[3] == 0.0f &&
              stiff[2] == 0.0f && stiff[3] > 0.2f && stiff[3] < 0.35f,
          "the shipped GroundEffectDoodadWind.cdbc loads its 37 rows: a doodad with the no wind and no push flags is "
          "still, a stiff one that only has no wind still leans from units, less than a flexible one");

    GrassDoodads missing;
    Check(!missing.LoadOverrides("no-such-folder\\GroundEffectDoodadWind.cdbc", error) && missing.OverrideCount() == 0,
          "a missing override file leaves every doodad on its automatic values");
}

const NearbyUnit* Unit(const NearbyUnitFrame& frame, uint64_t guid)
{
    for (uint32_t i = 0; i < frame.count; ++i)
        if (frame.units[i].guid == guid)
            return &frame.units[i];
    return nullptr;
}

uintptr_t MakeSlot(Arena& arena, const std::vector<uint32_t>& ids, std::vector<uint32_t>* colours = nullptr)
{
    const uintptr_t slot = arena.Allocate(kSlotBytes);
    const uintptr_t instances = arena.Allocate(ids.size() * kInstanceBytes);
    for (size_t i = 0; i < ids.size(); ++i)
    {
        Put<uint32_t>(instances + i * kInstanceBytes + 0x04, ids[i]);
        const uint32_t colour = kBaseColour + static_cast<uint32_t>(i) * 0x00010101u;
        Put<uint32_t>(instances + i * kInstanceBytes + 0x28, colour);
        if (colours)
            colours->push_back(colour);
    }
    Put<uint32_t>(slot + 0x18, static_cast<uint32_t>(ids.size()));
    Put<uintptr_t>(slot + 0x1C, instances);
    return slot;
}

uint32_t InstanceColour(uintptr_t slot, size_t index)
{
    return Get<uint32_t>(Get<uintptr_t>(slot + 0x1C) + index * kInstanceBytes + 0x28);
}

void CheckSlotTagging()
{
    Arena arena;
    std::vector<uint32_t> original;
    const uintptr_t slot = MakeSlot(arena, {30, 31, 30, 32, 31}, &original);
    engine::SlotTagging tagging;
    const bool tagged = engine::TagGrassSlot(slot, tagging);
    bool entries = tagged && tagging.tagged == 5 && tagging.doodads.count == 3 && tagging.doodads.ids[0] == 30 &&
                   tagging.doodads.ids[1] == 31 && tagging.doodads.ids[2] == 32 && !tagging.overflow;
    const unsigned expected[] = {0, 1, 0, 2, 1};
    bool tags = true;
    bool otherBits = true;
    for (size_t i = 0; i < 5; ++i)
    {
        const uint32_t now = InstanceColour(slot, i);
        tags = tags && (now & kGrassTagMask) == GrassEntryTag(expected[i]);
        otherBits = otherBits && (now & ~kGrassTagMask) == (original[i] & ~kGrassTagMask);
    }
    Check(entries && tags && otherBits,
          "tagging a slot writes each instance's entry into the tag bits of its colour and nothing else");
    engine::UntagGrassSlot(slot, tagging);
    bool restored = true;
    for (size_t i = 0; i < 5; ++i)
        restored = restored && InstanceColour(slot, i) == original[i];
    Check(restored, "untagging a slot gives every instance its own colour back");

    std::vector<uint32_t> many;
    for (uint32_t i = 0; i < kGrassSlotEntries + 5; ++i)
        many.push_back(500 + i);
    const uintptr_t crowded = MakeSlot(arena, many);
    engine::SlotTagging crowd;
    const bool crowdTagged = engine::TagGrassSlot(crowded, crowd);
    bool beyondFallback = true;
    for (size_t i = kGrassSlotEntries; i < many.size(); ++i)
        beyondFallback =
            beyondFallback && (InstanceColour(crowded, i) & kGrassTagMask) == GrassEntryTag(kGrassFallbackEntry);
    Check(crowdTagged && crowd.overflow && crowd.doodads.count == kGrassSlotEntries && beyondFallback,
          "a slot with more kinds of doodads than entries tags the extra ones with the fallback entry");

    const NoAccessPage page;
    const uintptr_t none = arena.Allocate(kSlotBytes);
    const uintptr_t unreadable = arena.Allocate(kSlotBytes);
    Put<uint32_t>(unreadable + 0x18, 4);
    Put<uintptr_t>(unreadable + 0x1C, page.Address());
    const uintptr_t huge = arena.Allocate(kSlotBytes);
    Put<uint32_t>(huge + 0x18, 1u << 20);
    Put<uintptr_t>(huge + 0x1C, Get<uintptr_t>(slot + 0x1C));
    engine::SlotTagging refused;
    Check(!engine::TagGrassSlot(none, refused) && !engine::TagGrassSlot(unreadable, refused) &&
              refused.tagged == 0 && !engine::TagGrassSlot(huge, refused),
          "a slot with no instances, an instance list that cannot be read or an absurd count is left alone");
}

void CheckPlantTagging()
{
    Arena arena;
    std::vector<uint32_t> original;
    const uintptr_t slot = MakeSlot(arena, {30, 31, 30}, &original);
    const uintptr_t instances = Get<uintptr_t>(slot + 0x1C);
    const float positions[3][2] = {{10.5f, 20.25f}, {-3.0f, 7.75f}, {10.5f, 20.25f}};
    for (size_t i = 0; i < 3; ++i)
    {
        Put<float>(instances + i * kInstanceBytes + 0x08, positions[i][0]);
        Put<float>(instances + i * kInstanceBytes + 0x0C, positions[i][1]);
    }
    engine::SlotTagging tagging;
    const bool tagged = engine::TagGrassSlot(slot, tagging, true);
    bool bitsRight = tagged;
    bool restOfColour = tagged;
    for (size_t i = 0; i < 3; ++i)
    {
        const uint32_t now = InstanceColour(slot, i);
        const uint32_t expectedBits = GrassPlantBits(positions[i][0], positions[i][1]);
        bitsRight = bitsRight && ((now & kGrassPlantMask) >> kGrassPlantShift) == expectedBits;
        restOfColour = restOfColour && (now & ~(kGrassTagMask | kGrassPlantMask)) ==
                                           (original[i] & ~(kGrassTagMask | kGrassPlantMask));
    }
    const bool topBitsKept = (InstanceColour(slot, 0) & 0xC0000000u) == (original[0] & 0xC0000000u);
    const bool samePlace = (InstanceColour(slot, 0) & kGrassPlantMask) == (InstanceColour(slot, 2) & kGrassPlantMask);
    Check(bitsRight && restOfColour && topBitsKept && samePlace,
          "tagging with random bits writes each plant's six position bits into the low bits of its alpha and leaves "
          "the lit or shadowed top bits and every colour bit that is not a tag alone");
    engine::UntagGrassSlot(slot, tagging);
    bool restored = true;
    for (size_t i = 0; i < 3; ++i)
        restored = restored && InstanceColour(slot, i) == original[i];
    Check(restored, "untagging gives every colour back, alpha included");

    const uintptr_t plain = MakeSlot(arena, {30, 31}, &original);
    engine::SlotTagging plainTagging;
    engine::TagGrassSlot(plain, plainTagging);
    Check((InstanceColour(plain, 0) & kGrassPlantMask) == (original[0] & kGrassPlantMask),
          "without random bits tagging leaves the alpha alone");
}

void CheckModelReading()
{
    ClientDoodads client(20);
    client.Add(kBladeId, Blade(1.0f, 0.9f, 0.1f));
    client.Add(kFlatId, Blade(0.05f, 0.5f, 0.1f));
    client.Add(10, Blade(1.0f, 0.9f, 0.1f), false);
    ModelBands blade = {};
    ModelBands flat = {};
    ModelBands notLoaded = {};
    ModelBands missing = {};
    ModelBands outside = {};
    const bool readBlade = engine::ReadGrassDoodadBandsFrom(client.listCell, client.sizeCell, kBladeId, blade);
    const bool readFlat = engine::ReadGrassDoodadBandsFrom(client.listCell, client.sizeCell, kFlatId, flat);
    Check(readBlade && blade.usable && std::fabs(blade.rootV - 0.9f) < kTolerance &&
              std::fabs(blade.tipV - 0.1f) < kTolerance && readFlat && flat.height < 0.15f,
          "a doodad's root and tip are read from the vertices its skin lists in the client's model data");
    Check(!engine::ReadGrassDoodadBandsFrom(client.listCell, client.sizeCell, 10, notLoaded) &&
              !engine::ReadGrassDoodadBandsFrom(client.listCell, client.sizeCell, 11, missing) &&
              !engine::ReadGrassDoodadBandsFrom(client.listCell, client.sizeCell, 20, outside),
          "a model the client has not finished loading, a doodad without an entry and an id past the list read "
          "nothing");

    ClientDoodads odd(4);
    odd.Add(1, Blade(1.0f, 0.9f, 0.1f));
    const uintptr_t entry = Get<uintptr_t>(Get<uintptr_t>(odd.listCell) + 1 * sizeof(uintptr_t));
    const uintptr_t shared = Get<uintptr_t>(Get<uintptr_t>(entry + 4) + 0x2C);
    const uintptr_t skin = Get<uintptr_t>(shared + 0x170);
    const uintptr_t lookup = Get<uintptr_t>(skin + 8);
    Put<uint16_t>(lookup, 500);
    ModelBands bad = {};
    Check(!engine::ReadGrassDoodadBandsFrom(odd.listCell, odd.sizeCell, 1, bad),
          "a skin that lists a vertex beyond the model's own count is refused");
    Put<uint16_t>(lookup, 0);
    Put<uint32_t>(skin + 4, 0);
    Check(!engine::ReadGrassDoodadBandsFrom(odd.listCell, odd.sizeCell, 1, bad),
          "a skin with no vertices is refused");
    Put<uint32_t>(skin + 4, 100000);
    Check(!engine::ReadGrassDoodadBandsFrom(odd.listCell, odd.sizeCell, 1, bad),
          "a skin with an absurd vertex count is refused");
    const NoAccessPage page;
    Put<uintptr_t>(Get<uintptr_t>(entry + 4) + 0x2C, page.Address());
    Check(!engine::ReadGrassDoodadBandsFrom(odd.listCell, odd.sizeCell, 1, bad),
          "model data at an address that cannot be read is refused without a crash");
}

void CheckGuards()
{
    size_t count = 0;
    const ClientCodeGuard* guards = engine::GrassDoodadGuards(count);
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
    bool everyByteMatters = engine::GrassDoodadCodeMatches(view);
    for (size_t i = 0; i < count; ++i)
    {
        std::vector<unsigned char> changed = bytes;
        changed[guards[i].address - low + guards[i].size - 1] ^= 0xFF;
        const ClientCodeView altered = {low, changed.data(), changed.size()};
        everyByteMatters = everyByteMatters && !engine::GrassDoodadCodeMatches(altered);
    }
    Check(everyByteMatters,
          "the grass doodad hooks need every guarded byte of the client's fill, draw and model code");
}

struct DoodadCalls
{
    int fills = 0;
    int draws = 0;
    int passEnds = 0;
    void* fillSlot = nullptr;
    void* drawSlot = nullptr;
    std::vector<uint32_t> coloursSeen;
    int uploadsBeforeDraw = 0;
};

DoodadCalls g_doodadCalls;
std::vector<float> g_uploadedTable;
unsigned g_tableUploads = 0;
unsigned g_tableFirst = 0;
unsigned g_tableCount = 0;
unsigned g_restoreUploads = 0;
unsigned g_restoreFirst = 0;
unsigned g_restoreCount = 0;
std::vector<float> g_restored;
unsigned g_otherUploads = 0;

void __fastcall FakeFill(void* slot, void*)
{
    ++g_doodadCalls.fills;
    g_doodadCalls.fillSlot = slot;
    g_doodadCalls.coloursSeen.clear();
    const uintptr_t address = reinterpret_cast<uintptr_t>(slot);
    const uint32_t count = Get<uint32_t>(address + 0x18);
    for (uint32_t i = 0; i < count; ++i)
        g_doodadCalls.coloursSeen.push_back(InstanceColour(address, i));
}

void __fastcall FakeDraw(void* slot, void*)
{
    ++g_doodadCalls.draws;
    g_doodadCalls.drawSlot = slot;
    g_doodadCalls.uploadsBeforeDraw = static_cast<int>(g_tableUploads);
}

void __cdecl FakePassEnd()
{
    ++g_doodadCalls.passEnds;
}

void RecordUpload(unsigned first, const float* data, unsigned count)
{
    if (first == kGrassDoodadFirstRegister && count == kGrassDoodadEntries)
    {
        ++g_tableUploads;
        g_tableFirst = first;
        g_tableCount = count;
        g_uploadedTable.assign(data, data + count * 4);
    }
    else if (first == engine::kGrassGridFirstRegister && count == kGrassDoodadFirstRegister + kGrassDoodadEntries -
                                                                      engine::kGrassGridFirstRegister)
    {
        ++g_restoreUploads;
        g_restoreFirst = first;
        g_restoreCount = count;
        g_restored.assign(data, data + count * 4);
    }
    else
        ++g_otherUploads;
}

bool FakeBands(uint32_t id, ModelBands& out)
{
    if (id == kBladeId)
        out = {true, 1.0f, 0.9f, 0.1f};
    else if (id == kFlatId)
        out = {true, 0.05f, 0.5f, 0.1f};
    else
        return false;
    return true;
}

bool FakeShadow(unsigned first, unsigned count, float* out)
{
    for (unsigned i = 0; i < count * 4; ++i)
        out[i] = static_cast<float>(first * 4 + i) + 0.25f;
    return true;
}

void ResetDoodadCalls()
{
    g_doodadCalls = {};
    g_uploadedTable.clear();
    g_restored.clear();
    g_tableUploads = g_restoreUploads = g_otherUploads = 0;
}

void CheckThunks()
{
    ResetDoodadCalls();
    float block[grass_wind_checks::kUploadedFloats] = {};
    const void* setupThunk = nullptr;
    const GrassInputs inputs = {};
    vf_test_grass_thunks(0, 0, block, &inputs, &RecordUpload, &setupThunk);
    const void* drawThunk = nullptr;
    const void* passEndThunk = nullptr;
    const void* fillThunk = vf_test_grass_doodad_thunks(reinterpret_cast<uintptr_t>(&FakeFill),
                                                        reinterpret_cast<uintptr_t>(&FakeDraw),
                                                        reinterpret_cast<uintptr_t>(&FakePassEnd), &FakeBands,
                                                        &FakeShadow, 1, &drawThunk, &passEndThunk);
    Arena arena;
    std::vector<uint32_t> original;
    const uintptr_t slot = MakeSlot(arena, {kFlatId, kBladeId, kFlatId, 9999}, &original);
    void* key = reinterpret_cast<void*>(slot);

    reinterpret_cast<void(__fastcall*)(void*)>(drawThunk)(key);
    float fallback[kGrassDoodadEntries][4];
    GrassDoodads().BuildTable(nullptr, fallback);
    bool fallbackTable = g_tableUploads == 1 && g_tableFirst == kGrassDoodadFirstRegister &&
                         g_tableCount == kGrassDoodadEntries;
    for (size_t i = 0; fallbackTable && i < g_uploadedTable.size(); ++i)
        fallbackTable = g_uploadedTable[i] == (&fallback[0][0])[i];
    Check(g_doodadCalls.draws == 1 && g_doodadCalls.drawSlot == key && g_doodadCalls.uploadsBeforeDraw == 1 &&
              fallbackTable,
          "before the client draws a slot nobody knows the thunk uploads a table of fallback entries, then runs the "
          "client's draw with the slot");

    reinterpret_cast<void(__fastcall*)(void*)>(fillThunk)(key);
    bool tagsSeen = g_doodadCalls.coloursSeen.size() == 4;
    const unsigned entries[] = {0, 1, 0, 2};
    for (size_t i = 0; tagsSeen && i < 4; ++i)
        tagsSeen = (g_doodadCalls.coloursSeen[i] & kGrassTagMask) == GrassEntryTag(entries[i]);
    bool restored = true;
    for (size_t i = 0; i < 4; ++i)
        restored = restored && InstanceColour(slot, i) == original[i];
    Check(g_doodadCalls.fills == 1 && g_doodadCalls.fillSlot == key && tagsSeen && restored,
          "the fill thunk tags the slot's colours while the client builds its vertices and gives them back after");

    GrassDoodads expected;
    expected.Learn(kFlatId, {true, 0.05f, 0.5f, 0.1f});
    expected.Learn(kBladeId, {true, 1.0f, 0.9f, 0.1f});
    GrassSlotDoodads known = {};
    known.ids[0] = kFlatId;
    known.ids[1] = kBladeId;
    known.ids[2] = 9999;
    known.count = 3;
    float table[kGrassDoodadEntries][4];
    expected.BuildTable(&known, table);
    bool tableMatches = g_tableUploads == 2 && g_uploadedTable.size() == kGrassDoodadEntries * 4;
    for (size_t i = 0; tableMatches && i < g_uploadedTable.size(); ++i)
        tableMatches = std::fabs(g_uploadedTable[i] - (&table[0][0])[i]) < kTolerance;
    Check(tableMatches && g_uploadedTable[2] == 0.0f,
          "after the fill the thunk uploads the slot's own table: the measured doodads in their entries, the flat "
          "one with no wind, an unreadable one on the fallback rule");

    reinterpret_cast<void(__fastcall*)(void*)>(drawThunk)(key);
    Check(g_doodadCalls.draws == 2 && g_tableUploads == 3 && std::fabs(g_uploadedTable[4] - 0.9f) < kTolerance,
          "a later draw of the same slot uploads the same table before the client's draw");

    reinterpret_cast<void(__cdecl*)()>(passEndThunk)();
    bool shadowCopy = g_restoreUploads == 1 && g_restoreFirst == engine::kGrassGridFirstRegister &&
                      g_restoreCount == 112 && g_restored.size() == 112 * 4;
    for (size_t i = 0; shadowCopy && i < g_restored.size(); ++i)
        shadowCopy = g_restored[i] == static_cast<float>(engine::kGrassGridFirstRegister * 4 + i) + 0.25f;
    Check(g_doodadCalls.passEnds == 1 && shadowCopy,
          "at the end of the grass pass the thunk puts back the client's own values for c40 to c151 before the "
          "client's next draw");
    reinterpret_cast<void(__cdecl*)()>(passEndThunk)();
    Check(g_doodadCalls.passEnds == 2 && g_restoreUploads == 1,
          "a pass that put nothing on the device puts nothing back");

    const unsigned before = g_tableUploads;
    ResetDoodadCalls();
    const void* offDrawThunk = nullptr;
    const void* offPassEndThunk = nullptr;
    const void* offFillThunk = vf_test_grass_doodad_thunks(reinterpret_cast<uintptr_t>(&FakeFill),
                                                           reinterpret_cast<uintptr_t>(&FakeDraw),
                                                           reinterpret_cast<uintptr_t>(&FakePassEnd), &FakeBands,
                                                           &FakeShadow, 0, &offDrawThunk, &offPassEndThunk);
    const uintptr_t plain = MakeSlot(arena, {kBladeId, kFlatId}, &original);
    void* plainKey = reinterpret_cast<void*>(plain);
    reinterpret_cast<void(__fastcall*)(void*)>(offFillThunk)(plainKey);
    bool untouched = g_doodadCalls.coloursSeen.size() == 2;
    for (size_t i = 0; untouched && i < 2; ++i)
        untouched = g_doodadCalls.coloursSeen[i] == InstanceColour(plain, i);
    reinterpret_cast<void(__fastcall*)(void*)>(offDrawThunk)(plainKey);
    float offFallback[kGrassDoodadEntries][4];
    GrassDoodads().BuildTable(nullptr, offFallback);
    bool offTable = g_tableUploads == 1;
    for (size_t i = 0; offTable && i < g_uploadedTable.size(); ++i)
        offTable = g_uploadedTable[i] == (&offFallback[0][0])[i];
    Check(before > 0 && untouched && offTable && g_doodadCalls.fills == 1 && g_doodadCalls.draws == 1,
          "with the bend by model off the fill leaves the colours alone and the draw still gets a fallback table");
    vf_test_grass_doodad_thunks(0, 0, 0, nullptr, nullptr, 1, &offDrawThunk, &offPassEndThunk);
}

void CheckGrassDoodads()
{
    CheckTags();
    CheckSlotEntries();
    CheckBands();
    CheckEntries();
    CheckTables();
    CheckOverrideFile();
    CheckSlotTagging();
    CheckPlantTagging();
    CheckModelReading();
    CheckGuards();
    CheckThunks();
}
}

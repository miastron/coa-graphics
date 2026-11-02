/*
 * Copyright (C) 2026 Morfium-G
 * Derived from seyris-living-azeroth (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU
 * General Public License version 3; see LICENSE.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

constexpr unsigned kGrassDoodadFirstRegister = 120;
constexpr unsigned kGrassDoodadEntries = 32;
constexpr unsigned kGrassSlotEntries = kGrassDoodadEntries - 1;
constexpr unsigned kGrassFallbackEntry = kGrassDoodadEntries - 1;
constexpr uint32_t kGrassTagMask = 0x00030703u;
constexpr size_t kGrassMaxSlots = 50000;

inline uint32_t GrassEntryTag(unsigned index)
{
    const uint32_t low = index & 7u;
    const uint32_t high = (index >> 3) & 3u;
    return (low << 8) | high | (high << 16);
}

struct GrassSlotDoodads
{
    uint32_t ids[kGrassSlotEntries];
    unsigned count;
};

unsigned GrassEntryFor(GrassSlotDoodads& slot, uint32_t id, bool& overflow);

struct ModelVertex
{
    float height;
    float v;
};

struct ModelBands
{
    bool usable;
    float height;
    float rootV;
    float tipV;
};

ModelBands GrassModelBands(const ModelVertex* vertices, size_t count);

enum GrassDoodadFlags : uint32_t
{
    kGrassNoWind = 0x01,
    kGrassNoPush = 0x02,
    kGrassFlip = 0x10
};

struct GrassDoodadOverride
{
    uint32_t flags = 0;
    float stiffness = 0.0f;
    float rootV = -1.0f;
    float tipV = -1.0f;
};

struct GrassDoodadInfo
{
    bool analyzed = false;
    bool valid = false;
    bool flat = false;
    float height = 0.0f;
    float rootV = 1.0f;
    float tipV = 0.0f;
    bool hasOverride = false;
    bool windOn = false;
    bool pushOn = false;
    bool flipped = false;
    float stiffness = 0.0f;
    float effectiveRootV = 1.0f;
    float effectiveTipV = 0.0f;
};

using GrassOverrides = std::unordered_map<uint32_t, GrassDoodadOverride>;

class GrassDoodads
{
public:
    bool LoadOverrides(const std::string& path, std::string& error);
    void SetOverrides(GrassOverrides overrides);
    size_t OverrideCount() const { return m_overrides.size(); }
    size_t Known() const { return m_doodads.size(); }
    bool Analyzed(uint32_t id) const;
    void Learn(uint32_t id, const ModelBands& bands);
    const GrassDoodadInfo* Info(uint32_t id) const;
    void Entry(uint32_t id, float (&out)[4]) const;
    void BuildTable(const GrassSlotDoodads* slot, float (&table)[kGrassDoodadEntries][4]) const;
    void RecordSlot(const void* slot, const GrassSlotDoodads& doodads);
    void ForgetSlot(const void* slot);
    const GrassSlotDoodads* SlotDoodads(const void* slot) const;
    void Clear();

private:
    void Resolve(uint32_t id, GrassDoodadInfo& info) const;

    std::unordered_map<uint32_t, GrassDoodadInfo> m_doodads;
    GrassOverrides m_overrides;
    std::unordered_map<const void*, GrassSlotDoodads> m_slots;
};

GrassDoodads& GlobalGrassDoodads();

/*
 * Copyright (C) 2026 Morfium-G
 * Derived from seyris-living-azeroth (https://github.com/Morfium-G/seyris-living-azeroth), licensed under the GNU
 * General Public License version 3; see LICENSE.
 */
#include "grass_doodads.h"

#include "wdbc.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kRootTipBand = 0.12f;
constexpr float kFlatHeight = 0.15f;
constexpr float kMinRootTipSpan = 0.01f;
constexpr float kMinSpanForReciprocal = 1.0e-4f;
constexpr uint32_t kOverrideColumns = 5;
constexpr uint32_t kOverrideIdColumn = 0;
constexpr uint32_t kOverrideFlagsColumn = 1;
constexpr uint32_t kOverrideStiffnessColumn = 2;
constexpr uint32_t kOverrideRootColumn = 3;
constexpr uint32_t kOverrideTipColumn = 4;

void FallbackEntry(float (&out)[4])
{
    out[0] = 1.0f;
    out[1] = -1.0f;
    out[2] = 1.0f;
    out[3] = 1.0f;
}
}

unsigned GrassEntryFor(GrassSlotDoodads& slot, uint32_t id, bool& overflow)
{
    for (unsigned k = 0; k < slot.count; ++k)
        if (slot.ids[k] == id)
            return k;
    if (slot.count < kGrassSlotEntries)
    {
        slot.ids[slot.count] = id;
        return slot.count++;
    }
    overflow = true;
    return kGrassFallbackEntry;
}

ModelBands GrassModelBands(const ModelVertex* vertices, size_t count)
{
    ModelBands bands = {false, 0.0f, 1.0f, 0.0f};
    if (!vertices || !count)
        return bands;
    float lowest = vertices[0].height;
    float highest = vertices[0].height;
    for (size_t i = 1; i < count; ++i)
    {
        lowest = std::min(lowest, vertices[i].height);
        highest = std::max(highest, vertices[i].height);
    }
    const float band = (highest - lowest) * kRootTipBand;
    float rootSum = 0.0f;
    float tipSum = 0.0f;
    size_t rootCount = 0;
    size_t tipCount = 0;
    for (size_t i = 0; i < count; ++i)
    {
        if (vertices[i].height <= lowest + band)
        {
            rootSum += vertices[i].v;
            ++rootCount;
        }
        if (vertices[i].height >= highest - band)
        {
            tipSum += vertices[i].v;
            ++tipCount;
        }
    }
    if (!rootCount || !tipCount)
        return bands;
    bands.usable = std::isfinite(rootSum) && std::isfinite(tipSum) && std::isfinite(highest - lowest);
    bands.height = highest - lowest;
    bands.rootV = rootSum / rootCount;
    bands.tipV = tipSum / tipCount;
    return bands;
}

bool GrassDoodads::LoadOverrides(const std::string& path, std::string& error)
{
    WdbcTable table;
    if (!table.Load(path, error))
        return false;
    if (table.Fields() < kOverrideColumns)
    {
        error = "GroundEffectDoodadWind has too few columns";
        return false;
    }
    GrassOverrides overrides;
    for (uint32_t r = 0; r < table.Records(); ++r)
    {
        GrassDoodadOverride row;
        row.flags = table.Raw(r, kOverrideFlagsColumn);
        row.stiffness = table.Float(r, kOverrideStiffnessColumn);
        row.rootV = table.Float(r, kOverrideRootColumn);
        row.tipV = table.Float(r, kOverrideTipColumn);
        overrides[table.Raw(r, kOverrideIdColumn)] = row;
    }
    SetOverrides(std::move(overrides));
    return true;
}

void GrassDoodads::SetOverrides(GrassOverrides overrides)
{
    m_overrides = std::move(overrides);
    for (auto& [id, info] : m_doodads)
        Resolve(id, info);
}

void GrassDoodads::Resolve(uint32_t id, GrassDoodadInfo& info) const
{
    info.effectiveRootV = info.rootV;
    info.effectiveTipV = info.tipV;
    const auto found = m_overrides.find(id);
    info.hasOverride = found != m_overrides.end();
    if (!info.hasOverride)
    {
        info.windOn = info.valid && !info.flat;
        info.pushOn = info.windOn;
        info.flipped = false;
        info.stiffness = 0.0f;
        return;
    }
    const GrassDoodadOverride& row = found->second;
    if (row.rootV >= 0.0f)
        info.effectiveRootV = row.rootV;
    if (row.tipV >= 0.0f)
        info.effectiveTipV = row.tipV;
    const bool usable = info.valid || (row.rootV >= 0.0f && row.tipV >= 0.0f);
    info.windOn = usable && !(row.flags & kGrassNoWind);
    info.pushOn = usable && !(row.flags & kGrassNoPush);
    info.flipped = (row.flags & kGrassFlip) != 0;
    info.stiffness = std::min(std::max(row.stiffness, 0.0f), 1.0f);
}

bool GrassDoodads::Analyzed(uint32_t id) const
{
    const auto found = m_doodads.find(id);
    return found != m_doodads.end() && found->second.analyzed;
}

void GrassDoodads::Learn(uint32_t id, const ModelBands& bands)
{
    GrassDoodadInfo& info = m_doodads[id];
    info.analyzed = true;
    info.valid = false;
    info.flat = false;
    if (bands.usable)
    {
        info.height = bands.height;
        info.rootV = bands.rootV;
        info.tipV = bands.tipV;
        info.flat = bands.height < kFlatHeight;
        info.valid = std::fabs(bands.tipV - bands.rootV) > kMinRootTipSpan || info.flat;
    }
    Resolve(id, info);
}

const GrassDoodadInfo* GrassDoodads::Info(uint32_t id) const
{
    const auto found = m_doodads.find(id);
    return found == m_doodads.end() ? nullptr : &found->second;
}

void GrassDoodads::Entry(uint32_t id, float (&out)[4]) const
{
    FallbackEntry(out);
    const GrassDoodadInfo* info = Info(id);
    if (!info || !info->analyzed || (!info->valid && !info->hasOverride))
        return;
    const float rootV = info->flipped ? info->effectiveTipV : info->effectiveRootV;
    const float tipV = info->flipped ? info->effectiveRootV : info->effectiveTipV;
    const float span = tipV - rootV;
    const float flex = 1.0f - info->stiffness;
    out[0] = rootV;
    out[1] = std::fabs(span) > kMinSpanForReciprocal ? 1.0f / span : 0.0f;
    out[2] = info->windOn ? flex : 0.0f;
    out[3] = info->pushOn ? flex : 0.0f;
}

void GrassDoodads::BuildTable(const GrassSlotDoodads* slot, float (&table)[kGrassDoodadEntries][4]) const
{
    for (unsigned k = 0; k < kGrassDoodadEntries; ++k)
        FallbackEntry(table[k]);
    if (!slot)
        return;
    for (unsigned k = 0; k < slot->count && k < kGrassSlotEntries; ++k)
        Entry(slot->ids[k], table[k]);
}

void GrassDoodads::RecordSlot(const void* slot, const GrassSlotDoodads& doodads)
{
    if (m_slots.size() > kGrassMaxSlots)
        m_slots.clear();
    m_slots[slot] = doodads;
}

void GrassDoodads::ForgetSlot(const void* slot)
{
    m_slots.erase(slot);
}

const GrassSlotDoodads* GrassDoodads::SlotDoodads(const void* slot) const
{
    const auto found = m_slots.find(slot);
    return found == m_slots.end() ? nullptr : &found->second;
}

void GrassDoodads::Clear()
{
    m_doodads.clear();
    m_slots.clear();
}

GrassDoodads& GlobalGrassDoodads()
{
    static GrassDoodads doodads;
    return doodads;
}

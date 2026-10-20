/*
 * Copyright (C) 2026 WarcraftXL
 * Derived from wxl-treewind (https://github.com/manatactical/wxl-treewind), licensed under the GNU General Public
 * License version 3; see LICENSE.
 */
#pragma once

#include "client_code.h"
#include "config.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

struct TreeWindWave
{
    float directionDeg;
    float speed;
    float amplitudeDeg;
    float wavelength;
    float crossAmplitudeDeg;
    float crossWavelength;
    float crossAngleDeg;
    float leanDeg;
    float variance;
    float gust;
    float distanceFade;
};

struct TreeWindSettings
{
    bool trees;
    TreeWindWave treeWave;
    bool treeDoodadsOnly;
    bool treeMatchNames;
    bool treeMatchTallThin;
    bool treeSkipDead;
    float treeMinHeight;
    float treeMinAspect;
    float treeMaxHeight;
    float treeMaxDistance;
    bool bushes;
    TreeWindWave bushWave;
    bool bushDoodadsOnly;
    bool bushSkipDead;
    float bushMaxDistance;
};

TreeWindSettings TreeWindSettingsFrom(const Config& config);

namespace engine
{
constexpr uintptr_t kModelAnimateFull = 0x0082F0F0;
constexpr uintptr_t kModelAnimateSimple = 0x0082E140;
constexpr uintptr_t kModelAnimateFullSites[] = {0x0081CEFA, 0x00821B4E, 0x00821BD8, 0x0082E6FD,
                                                0x00830D8A, 0x00830E4C, 0x00830F27};
constexpr uintptr_t kModelAnimateSimpleSites[] = {0x0081CECF, 0x00821B21, 0x00821BAD, 0x0082E680,
                                                  0x0082E6E2, 0x00830E1F, 0x00830F20};

constexpr uintptr_t kModelFlags = 0x04;
constexpr uintptr_t kModelShared = 0x2C;
constexpr uintptr_t kModelPlacement = 0xB4;
constexpr uint32_t kMapDoodadModelFlag = 0x20;
constexpr uintptr_t kSharedFileName = 0x3C;
constexpr size_t kSharedFileNameSize = 260;
constexpr uintptr_t kSharedFileStem = 0x140;
constexpr uintptr_t kSharedData = 0x150;
constexpr uintptr_t kSharedBounds = 0x154;
constexpr size_t kPlacementFloats = 12;

const ClientCodeGuard* TreeWindGuards(size_t& count);
bool TreeWindCodeMatches(const ClientCodeView& code);
}

class TreeWind
{
public:
    void Configure(const TreeWindSettings& settings);
    bool Sway(void* instance, const float camera[3], uint32_t tickMs);
    void Clear();
    size_t CachedInstances() const;
    bool HasSwayed() const { return m_swayed; }

private:
    struct BaseEntry
    {
        const void* instance = nullptr;
        uintptr_t model = 0;
        uint32_t stamp = 0;
        float base[engine::kPlacementFloats] = {};
    };

    bool BasePlacement(const void* instance, uintptr_t model, const float* current, uint32_t now,
                       float (&out)[engine::kPlacementFloats]);

    TreeWindSettings m_settings = TreeWindSettingsFrom(Config());
    std::vector<BaseEntry> m_cache;
    mutable std::mutex m_lock;
    bool m_swayed = false;
};

TreeWind& GlobalTreeWind();

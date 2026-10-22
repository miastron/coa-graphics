/*
 * Copyright (C) 2026 WarcraftXL
 * Derived from wxl-treewind (https://github.com/manatactical/wxl-treewind), licensed under the GNU General Public
 * License version 3; see LICENSE.
 */
#include "tree_wind.h"

#include "log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;
constexpr float kDegreesToRadians = kPi / 180.0f;
constexpr size_t kCacheSize = 8192;
constexpr size_t kProbeLimit = 32;
constexpr uint32_t kStaleMs = 2000;
constexpr uint32_t kWindClockWrapMs = 3600000;
constexpr float kMillisecondsToSeconds = 0.001f;
constexpr float kCrossWaveSpeedScale = 1.37f;
constexpr float kGustRate = 0.37f;
constexpr float kGustSeedShare = 0.5f;
constexpr float kCrossSeedShare = 0.6f;
constexpr float kMinWavelength = 0.1f;
constexpr float kNoTiltSquared = 1.0e-12f;
constexpr float kFlatFootprint = 0.01f;
constexpr size_t kPathMemoSize = 256;
constexpr int kPathHashLimit = 128;
constexpr int kPathScanLimit = 512;
constexpr int kWorldPositionCellsPerYard = 8;
constexpr size_t kPlacementRows = 3;
constexpr size_t kPlacementRowStride = 4;
constexpr size_t kTranslationX = 12;
constexpr size_t kTranslationY = 13;
constexpr size_t kTranslationZ = 14;

const ClientCodeGuard kTreeWindCode[] = {
    {"full model animate entry", engine::kModelAnimateFull, 16,
     {0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x9C, 0x01, 0x00, 0x00, 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x10, 0xA8}},
    {"full model animate return", 0x00830968, 3, {0xC2, 0x14, 0x00}},
    {"simple model animate entry", engine::kModelAnimateSimple, 16,
     {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x50, 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x10, 0xA8, 0x01, 0x0F, 0x84}},
    {"simple model animate return", 0x0082E545, 3, {0xC2, 0x14, 0x00}},
};

const char* const kTreeNames[] = {
    "tree", "pines", "pine", "spruce", "cedar", "oak",  "willow",     "birch",  "aspen",   "palm",    "canopy",
    "conifer", "maple", "eucalyptus", "redwood", "cypress", "larch", "juniper", "mangrove", "olive", "baobab",
    "treant",
};

const char* const kBushNames[] = {
    "bush", "shrub", "fern", "hedge", "thicket", "bramble", "undergrowth", "topiary", "bracken", "sapling",
};

const char* const kDeadNames[] = {
    "dead", "deadwood", "burnt", "burned", "charred", "scorch", "wither", "wilted", "blight", "corrupt",
    "cursed", "bare", "leafless", "defoliated", "skeletal", "petrified", "stump", "snag", "log", "driftwood",
    "uprooted", "felled", "fallen", "rotten", "hollow",
};

template <typename T>
T ReadAt(const void* base, uintptr_t offset)
{
    T value;
    std::memcpy(&value, static_cast<const char*>(base) + offset, sizeof(value));
    return value;
}

char Lower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool ContainsNoCase(const char* haystack, const char* needle)
{
    if (!haystack || !needle || !*needle)
        return false;
    for (int i = 0; haystack[i] && i < kPathScanLimit; ++i)
    {
        int h = i;
        int n = 0;
        while (haystack[h] && needle[n] && Lower(haystack[h]) == Lower(needle[n]))
        {
            ++h;
            ++n;
        }
        if (!needle[n])
            return true;
    }
    return false;
}

template <size_t N>
bool ContainsAny(const char* path, const char* const (&names)[N])
{
    for (const char* name : names)
        if (ContainsNoCase(path, name))
            return true;
    return false;
}

struct PathClass
{
    uintptr_t model = 0;
    uint32_t hash = 0;
    bool tree = false;
    bool bush = false;
    bool dead = false;
};

thread_local PathClass t_pathMemo[kPathMemoSize];

uint32_t PathHash(const char* text)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; text[i] && i < kPathHashLimit; ++i)
    {
        hash ^= static_cast<uint8_t>(text[i]);
        hash *= 16777619u;
    }
    return hash;
}

const PathClass& ClassifyPath(uintptr_t model, const char* path)
{
    PathClass& slot = t_pathMemo[(model >> 4) & (kPathMemoSize - 1)];
    const uint32_t hash = PathHash(path);
    if (slot.model != model || slot.hash != hash)
    {
        slot.model = model;
        slot.hash = hash;
        slot.dead = ContainsAny(path, kDeadNames);
        slot.bush = ContainsAny(path, kBushNames);
        slot.tree = ContainsAny(path, kTreeNames);
    }
    return slot;
}

struct Extents
{
    float x;
    float y;
    float z;
};

bool ReadExtents(const void* shared, Extents& out)
{
    float box[6];
    std::memcpy(box, static_cast<const char*>(shared) + engine::kSharedBounds, sizeof(box));
    out = {box[3] - box[0], box[4] - box[1], box[5] - box[2]};
    return out.x == out.x && out.y == out.y && out.z == out.z;
}

bool LooksTallThin(const Extents& e, const TreeWindSettings& s)
{
    if (e.z < s.treeMinHeight)
        return false;
    const float horizontal = e.x > e.y ? e.x : e.y;
    if (horizontal < kFlatFootprint)
        return true;
    return e.z / horizontal >= s.treeMinAspect;
}

float MaxAxisScaleSquared(const float* placement)
{
    float best = 0.0f;
    for (size_t row = 0; row < kPlacementRows; ++row)
    {
        const float* axis = placement + row * kPlacementRowStride;
        best = std::max(best, axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    }
    return best;
}

float Hash01(float x, float y)
{
    const uint32_t ix = static_cast<uint32_t>(static_cast<int32_t>(x * kWorldPositionCellsPerYard));
    const uint32_t iy = static_cast<uint32_t>(static_cast<int32_t>(y * kWorldPositionCellsPerYard));
    uint32_t h = ix * 73856093u ^ iy * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

void BuildTiltRowMatrix(float tiltX, float tiltY, float m[9])
{
    const float squared = tiltX * tiltX + tiltY * tiltY;
    if (squared < kNoTiltSquared)
    {
        std::memset(m, 0, sizeof(float) * 9);
        m[0] = m[4] = m[8] = 1.0f;
        return;
    }
    const float theta = std::sqrt(squared);
    const float ax = -tiltY / theta;
    const float ay = tiltX / theta;
    const float az = 0.0f;
    const float s = std::sin(theta);
    const float c = std::cos(theta);
    const float ic = 1.0f - c;
    m[0] = c + ax * ax * ic;
    m[1] = ay * ax * ic + az * s;
    m[2] = az * ax * ic - ay * s;
    m[3] = ax * ay * ic - az * s;
    m[4] = c + ay * ay * ic;
    m[5] = az * ay * ic + ax * s;
    m[6] = ax * az * ic + ay * s;
    m[7] = ay * az * ic - ax * s;
    m[8] = c + az * az * ic;
}

struct WindBasis
{
    float directionDeg = -1.0f;
    float crossDeg = -1.0f;
    float wavelength = -1.0f;
    float crossWavelength = -1.0f;
    float d1x = 1.0f;
    float d1y = 0.0f;
    float d2x = 1.0f;
    float d2y = 0.0f;
    float k1 = 1.0f;
    float k2 = 1.0f;
};

thread_local WindBasis t_treeBasis;
thread_local WindBasis t_bushBasis;

const WindBasis& WindBasisFor(WindBasis& cache, const TreeWindWave& w)
{
    if (cache.directionDeg != w.directionDeg || cache.crossDeg != w.crossAngleDeg ||
        cache.wavelength != w.wavelength || cache.crossWavelength != w.crossWavelength)
    {
        cache.directionDeg = w.directionDeg;
        cache.crossDeg = w.crossAngleDeg;
        cache.wavelength = w.wavelength;
        cache.crossWavelength = w.crossWavelength;
        const float a1 = w.directionDeg * kDegreesToRadians;
        const float a2 = (w.directionDeg + w.crossAngleDeg) * kDegreesToRadians;
        cache.k1 = kTwoPi / std::max(w.wavelength, kMinWavelength);
        cache.k2 = kTwoPi / std::max(w.crossWavelength, kMinWavelength);
        cache.d1x = std::cos(a1);
        cache.d1y = std::sin(a1);
        cache.d2x = std::cos(a2);
        cache.d2y = std::sin(a2);
    }
    return cache;
}

TreeWindWave TreeWave(const Config& c)
{
    return {c.treeWindDirection,      c.treeWindSpeed,           c.treeWindAmplitude, c.treeWindWavelength,
            c.treeWindCrossAmplitude, c.treeWindCrossWavelength, c.treeWindCrossAngle, c.treeWindLean,
            c.treeWindVariance,       c.treeWindGust,            c.treeWindDistanceFade};
}

TreeWindWave BushWave(const Config& c)
{
    return {c.bushWindDirection,      c.bushWindSpeed,           c.bushWindAmplitude, c.bushWindWavelength,
            c.bushWindCrossAmplitude, c.bushWindCrossWavelength, c.bushWindCrossAngle, c.bushWindLean,
            c.bushWindVariance,       c.bushWindGust,            c.bushWindDistanceFade};
}

struct Candidate
{
    const void* shared;
    uintptr_t model;
    const PathClass* path;
    uint32_t flags;
};

bool ReadCandidate(const void* instance, Candidate& out)
{
    const uintptr_t shared = ReadAt<uintptr_t>(instance, engine::kModelShared);
    if (!shared)
        return false;
    const void* sharedPointer = reinterpret_cast<const void*>(shared);
    if (!ReadAt<uintptr_t>(sharedPointer, engine::kSharedData))
        return false;
    const uintptr_t stem = ReadAt<uintptr_t>(sharedPointer, engine::kSharedFileStem);
    const uintptr_t nameBegin = shared + engine::kSharedFileName;
    if (stem < nameBegin || stem >= nameBegin + engine::kSharedFileNameSize)
        return false;
    out.shared = sharedPointer;
    out.model = shared;
    out.flags = ReadAt<uint32_t>(instance, engine::kModelFlags);
    out.path = &ClassifyPath(shared, reinterpret_cast<const char*>(stem));
    return true;
}

float ClampedUnit(float v)
{
    return std::min(std::max(v, 0.0f), 1.0f);
}
}

TreeWindSettings TreeWindSettingsFrom(const Config& c)
{
    return {c.treeWind,       TreeWave(c),       c.treeDoodadsOnly, c.treeMatchNames,  c.treeMatchTallThin,
            c.treeSkipDead,   c.treeMinHeight,   c.treeMinAspect,   c.treeMaxHeight,   c.treeMaxDistance,
            c.bushWind,       BushWave(c),       c.bushDoodadsOnly, c.bushSkipDead,    c.bushMaxDistance};
}

namespace engine
{
const ClientCodeGuard* TreeWindGuards(size_t& count)
{
    count = sizeof(kTreeWindCode) / sizeof(kTreeWindCode[0]);
    return kTreeWindCode;
}

bool TreeWindCodeMatches(const ClientCodeView& code)
{
    for (const ClientCodeGuard& guard : kTreeWindCode)
        if (!ClientGuardMatches(code, guard))
        {
            VF_LOG_ERROR("tree wind: the client's %s at 0x%08X differs from the 12340 client; trees do not sway",
                         guard.name, static_cast<unsigned>(guard.address));
            return false;
        }
    return true;
}
}

void TreeWind::Configure(const TreeWindSettings& settings)
{
    m_settings = settings;
}

void TreeWind::Clear()
{
    std::lock_guard<std::mutex> lock(m_lock);
    m_cache.clear();
}

size_t TreeWind::CachedInstances() const
{
    std::lock_guard<std::mutex> lock(m_lock);
    size_t count = 0;
    for (const BaseEntry& e : m_cache)
        count += e.instance != nullptr;
    return count;
}

bool TreeWind::BasePlacement(const void* instance, uintptr_t model, const float* current, uint32_t now,
                             float (&out)[engine::kPlacementFloats])
{
    std::lock_guard<std::mutex> lock(m_lock);
    if (m_cache.empty())
        m_cache.resize(kCacheSize);
    const size_t start = (reinterpret_cast<uintptr_t>(instance) >> 4) & (kCacheSize - 1);
    size_t victim = kCacheSize;
    uint32_t victimStamp = 0;
    auto seed = [&](BaseEntry& e) {
        e.instance = instance;
        e.model = model;
        e.stamp = now;
        std::memcpy(e.base, current, sizeof(e.base));
        std::memcpy(e.translation, current + kTranslationX, sizeof(e.translation));
        std::memcpy(out, e.base, sizeof(out));
    };
    for (size_t probe = 0; probe < kProbeLimit; ++probe)
    {
        const size_t index = (start + probe) & (kCacheSize - 1);
        BaseEntry& e = m_cache[index];
        if (e.instance == instance)
        {
            if (e.model != model || std::memcmp(e.translation, current + kTranslationX, sizeof(e.translation)) != 0)
                seed(e);
            e.stamp = now;
            std::memcpy(out, e.base, sizeof(out));
            return true;
        }
        if (e.instance == nullptr)
        {
            seed(e);
            return true;
        }
        if (victim == kCacheSize || e.stamp < victimStamp)
        {
            victimStamp = e.stamp;
            victim = index;
        }
    }
    if (victim < kCacheSize && now - victimStamp > kStaleMs)
    {
        seed(m_cache[victim]);
        return true;
    }
    return false;
}

bool TreeWind::Sway(void* instance, const float camera[3], uint32_t tickMs)
{
    const TreeWindSettings& s = m_settings;
    if (!instance || (!s.trees && !s.bushes))
        return false;

    Candidate candidate;
    if (!ReadCandidate(instance, candidate))
        return false;
    const PathClass& path = *candidate.path;
    const bool mapDoodad = (candidate.flags & engine::kMapDoodadModelFlag) != 0;

    const bool isBush = path.bush && s.bushes;
    const TreeWindWave& wave = isBush ? s.bushWave : s.treeWave;
    if (!isBush && !s.trees)
        return false;

    Extents extents = {};
    const bool haveExtents = ReadExtents(candidate.shared, extents);
    if (isBush)
    {
        if (s.bushDoodadsOnly && !mapDoodad)
            return false;
        if (s.bushSkipDead && path.dead)
            return false;
    }
    else
    {
        if (s.treeDoodadsOnly && !mapDoodad)
            return false;
        if (s.treeSkipDead && path.dead)
            return false;
        bool sway = s.treeMatchNames || s.treeMatchTallThin;
        if (s.treeMatchNames && !path.tree)
            sway = false;
        if (sway && s.treeMatchTallThin && !(haveExtents && LooksTallThin(extents, s)))
            sway = false;
        if (!sway)
            return false;
    }

    float* placement = reinterpret_cast<float*>(static_cast<char*>(instance) + engine::kModelPlacement);
    if (!isBush && s.treeMaxHeight > 0.0f && haveExtents && extents.z > 0.0f &&
        extents.z * extents.z * MaxAxisScaleSquared(placement) > s.treeMaxHeight * s.treeMaxHeight)
        return false;

    const float px = placement[kTranslationX];
    const float py = placement[kTranslationY];
    const float pz = placement[kTranslationZ];
    const float dx = px - camera[0];
    const float dy = py - camera[1];
    const float dz = pz - camera[2];
    const float distanceSquared = dx * dx + dy * dy + dz * dz;
    const float maxDistance = isBush ? s.bushMaxDistance : s.treeMaxDistance;
    if (maxDistance > 0.0f && distanceSquared > maxDistance * maxDistance)
        return false;

    float base[engine::kPlacementFloats];
    if (!BasePlacement(instance, candidate.model, placement, tickMs, base))
        return false;

    const float t = static_cast<float>(tickMs % kWindClockWrapMs) * kMillisecondsToSeconds;
    const WindBasis& basis = WindBasisFor(isBush ? t_bushBasis : t_treeBasis, wave);
    const float seed = Hash01(px, py) * kTwoPi;
    const float variance = ClampedUnit(wave.variance);
    const float amplitudeScale = 1.0f + (Hash01(py, px) - 0.5f) * 2.0f * variance;
    float fade = 1.0f;
    if (wave.distanceFade > 0.0f)
        fade = 1.0f / (1.0f + wave.distanceFade * std::sqrt(distanceSquared));

    const float phase1 = -basis.k1 * wave.speed * t;
    const float phase2 = -basis.k2 * wave.speed * kCrossWaveSpeedScale * t;
    const float gust = 1.0f + wave.gust * std::sin(t * kGustRate + seed * kGustSeedShare);
    const float s1 = std::sin(basis.k1 * (px * basis.d1x + py * basis.d1y) + phase1 + seed);
    const float s2 = std::sin(basis.k2 * (px * basis.d2x + py * basis.d2y) + phase2 + seed * kCrossSeedShare);
    const float degrees1 = (wave.amplitudeDeg * s1 * gust + wave.leanDeg) * amplitudeScale * fade;
    const float degrees2 = wave.crossAmplitudeDeg * s2 * gust * amplitudeScale * fade;
    const float tiltX = (degrees1 * basis.d1x + degrees2 * basis.d2x) * kDegreesToRadians;
    const float tiltY = (degrees1 * basis.d1y + degrees2 * basis.d2y) * kDegreesToRadians;

    float rotation[9];
    BuildTiltRowMatrix(tiltX, tiltY, rotation);
    for (size_t row = 0; row < kPlacementRows; ++row)
    {
        const float b0 = base[row * kPlacementRowStride + 0];
        const float b1 = base[row * kPlacementRowStride + 1];
        const float b2 = base[row * kPlacementRowStride + 2];
        placement[row * kPlacementRowStride + 0] = b0 * rotation[0] + b1 * rotation[3] + b2 * rotation[6];
        placement[row * kPlacementRowStride + 1] = b0 * rotation[1] + b1 * rotation[4] + b2 * rotation[7];
        placement[row * kPlacementRowStride + 2] = b0 * rotation[2] + b1 * rotation[5] + b2 * rotation[8];
    }
    m_swayed = true;
    return true;
}

TreeWind& GlobalTreeWind()
{
    static TreeWind treeWind;
    return treeWind;
}

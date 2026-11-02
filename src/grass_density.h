#pragma once

#include "config.h"

#include <cstddef>
#include <cstdint>

struct GrassDensitySettings
{
    float multiplier;
    float radius;
    float spread;
    float fade = 32.0f;
};

GrassDensitySettings GrassDensitySettingsFrom(const Config& config);

constexpr unsigned kGrassMaxExtraCopies = 3;
constexpr unsigned kGrassCopyRegister = 20;
constexpr unsigned kGrassCopyRegisterCount = 2;
constexpr size_t kWorldMatrixTranslationX = 12;
constexpr size_t kWorldMatrixTranslationY = 13;
constexpr float kGrassChunkSize = 33.333332f;
constexpr float kGrassFullDensityShare = 0.5f;
constexpr unsigned kGrassPlantBitCount = 6;
constexpr unsigned kGrassPlantShift = 24;
constexpr uint32_t kGrassPlantMask = ((1u << kGrassPlantBitCount) - 1) << kGrassPlantShift;
constexpr float kGrassPlantMagnitudeFloor = 0.6f;
constexpr float kGrassPlantMagnitudeRange = 0.4f;
constexpr float kGrassPlantMagnitudeMix = 5.3f;
constexpr float kGrassThresholdMix = 39.554175f;
constexpr float kGrassSinkDepth = 2.0f;
constexpr float kGrassWindowMinimum = 0.001f;

struct GrassCopyOffset
{
    float x;
    float y;
};

struct GrassCopyWindow
{
    float outerDistance;
    float width;
    float inverseShare;
    float inverseFade;
};

float GrassChunkDistance(const float* worldMatrix);
unsigned GrassCopyCount(const GrassDensitySettings& settings);
GrassCopyWindow GrassCopyWindowFor(const GrassDensitySettings& settings, unsigned copy);
float GrassPlantSink(const GrassCopyWindow& window, unsigned plantBits, float distance);
unsigned GrassExtraCopies(const GrassDensitySettings& settings, float chunkDistance);
unsigned GrassPlantBits(float x, float y);
GrassCopyOffset GrassPlantOffset(const GrassCopyOffset& copy, unsigned plantBits);
GrassCopyOffset GrassCopyOffsetFor(const GrassDensitySettings& settings, unsigned copy);

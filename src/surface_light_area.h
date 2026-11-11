#pragma once

#include <d3d9.h>

#include <cstdint>

constexpr float kFaintestSurfaceLight = 1.0f / 1024.0f;

struct SurfaceLight
{
    float viewPosition[3];
    float cutoff;
    float colour[3];
    float strength;
    float attenuation[3];
};

struct SurfaceLightArea
{
    RECT inViewport = {};
    float share = 0.0f;
    float shareOfLightsApart = 0.0f;

    bool Empty() const { return share <= 0.0f; }
};

float SurfaceLightRadius(const SurfaceLight& light, uint32_t lightsOnSurfaces);
SurfaceLightArea ProjectSurfaceLights(const SurfaceLight* lights, uint32_t count, const float* glProjection,
                                      const D3DVIEWPORT9& vp);
SurfaceLightArea WholeViewport(const D3DVIEWPORT9& vp);

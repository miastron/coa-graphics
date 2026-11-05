#pragma once

#include <d3d9.h>

constexpr float kLightInFrontViewZ = 0.05f;

struct LightOnScreen
{
    bool inFront;
    float viewZ;
    float ndc[2];
    float pixel[2];
};

LightOnScreen ProjectLightOntoViewport(const float* toLightInView, const float* glProjection, const D3DVIEWPORT9& vp);

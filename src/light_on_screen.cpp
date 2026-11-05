#include "light_on_screen.h"

LightOnScreen ProjectLightOntoViewport(const float* toLightInView, const float* glProjection, const D3DVIEWPORT9& vp)
{
    LightOnScreen light = {};
    light.viewZ = toLightInView[2];
    light.inFront = light.viewZ > kLightInFrontViewZ;
    if (!light.inFront)
        return light;
    light.ndc[0] = toLightInView[0] / toLightInView[2] * glProjection[0] + glProjection[8];
    light.ndc[1] = toLightInView[1] / toLightInView[2] * glProjection[5] + glProjection[9];
    light.pixel[0] = vp.X + (light.ndc[0] * 0.5f + 0.5f) * vp.Width;
    light.pixel[1] = vp.Y + (0.5f - light.ndc[1] * 0.5f) * vp.Height;
    return light;
}

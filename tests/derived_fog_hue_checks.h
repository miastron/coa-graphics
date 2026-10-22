#pragma once

namespace derived_fog_hue
{
constexpr uint32_t kElwynnNoonFogColor = 0xFF4C788E;
constexpr uint32_t kElwynnNoonDirectLight = 0xFFFE8E40;
constexpr float kMaxRedToBlueInElwynnScatter = 2.0f;
constexpr float kMaxDirectHueShare = 0.26f;
constexpr float kLuminanceTolerance = 1e-4f;
constexpr float kLuminanceWeights[3] = {0.2126f, 0.7152f, 0.0722f};
constexpr uint32_t kFogColours[] = {0xFF4C788E, 0xFF808080, 0xFF345466, 0xFF2A4A1E, 0xFF8A6A50, 0xFF101820};
constexpr uint32_t kDirectLights[] = {0xFFFE8E40, 0xFFFFFFFF, 0xFFC07E54, 0xFF40546A, 0xFFFF2010, 0xFF2040FF};

float Luma(const float* rgb)
{
    return rgb[0] * kLuminanceWeights[0] + rgb[1] * kLuminanceWeights[1] + rgb[2] * kLuminanceWeights[2];
}

float HueDistance(const float* a, const float* b)
{
    const float scaleA = 1.0f / Luma(a);
    const float scaleB = 1.0f / Luma(b);
    float sum = 0.0f;
    for (int c = 0; c < 3; ++c)
    {
        const float d = a[c] * scaleA - b[c] * scaleB;
        sum += d * d;
    }
    return std::sqrt(sum);
}

FrameInputs NoonFrame(uint32_t fogColour, uint32_t directLight)
{
    const Vec3 eye = Add(kGameLikeWorldOffset, {0, 0, 9});
    FrameInputs in = ContinentFrame(kEasternKingdoms, eye, kNoon, {0.0f, 0.0f, 1.0f}, false);
    in.fogColor = fogColour;
    in.directColor = directLight;
    return in;
}

void CheckElwynnNoonFogIsNotOrange()
{
    const FogParams fog = BuildFogParams(NoonFrame(kElwynnNoonFogColor, kElwynnNoonDirectLight), Config(), nullptr);
    for (const FogLayer* layer : {&fog.layers[0], &fog.layers[1], &fog.layers[kDistanceFogLayer]})
        Check(layer->diffuse[0] <= kMaxRedToBlueInElwynnScatter * layer->diffuse[2],
              "the derived and distance fog's sun scatter at Elwynn's noon (blue fog, orange direct light) is not orange");
}

void CheckSunScatterKeepsTheZonesFogHue()
{
    bool brightnessKept = true;
    bool hueAnchored = true;
    for (uint32_t fogColour : kFogColours)
        for (uint32_t directLight : kDirectLights)
        {
            const FogParams fog = BuildFogParams(NoonFrame(fogColour, directLight), Config(), nullptr);
            float fogRgb[3];
            UnpackColor(fogColour, fogRgb);
            float directRgb[3];
            UnpackColor(directLight, directRgb);
            for (int c = 0; c < 3; ++c)
            {
                fogRgb[c] = std::pow(fogRgb[c], 2.2f);
                directRgb[c] = std::pow(directRgb[c], 2.2f);
            }
            brightnessKept = brightnessKept &&
                             std::fabs(Luma(fog.sunScatterColor) - Luma(directRgb)) <= kLuminanceTolerance;
            hueAnchored = hueAnchored && HueDistance(fog.sunScatterColor, fogRgb) <=
                                             kMaxDirectHueShare * HueDistance(directRgb, fogRgb) + kLuminanceTolerance;
        }
    Check(brightnessKept, "the derived sun scatter keeps the client's direct light brightness for every zone colour");
    Check(hueAnchored, "the derived sun scatter's hue stays within a quarter of the way from the zone's fog colour");
}

void CheckDerivedFogHue()
{
    CheckElwynnNoonFogIsNotOrange();
    CheckSunScatterKeepsTheZonesFogHue();
}
}

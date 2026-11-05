#include "vf_common.hlsli"

float4 cRayMask : register(c9);
float4 cRaySource : register(c10);
float4 cCharacterBox : register(c11);
float4 cCharacterFocus : register(c12);
float4 cCharacterHeights : register(c13);

sampler2D sDepth : register(s0);
sampler2D sRaySource : register(s1);

static const float3 kLumaWeights = float3(0.299, 0.587, 0.114);
static const float kThresholdRampSlope = 4;
static const int kSourceTexelsPerRayTexel = 2;
static const int kDepthTapsPerSourceTexel = 2;
static const int kDepthTapsPerRayTexel = kSourceTexelsPerRayTexel * kDepthTapsPerSourceTexel;

float3 DirectionToSunView()
{
    return cRayMask.xyz;
}

float AngularFalloffExponent()
{
    return cRayMask.w;
}

float LuminanceThreshold()
{
    return cRaySource.y;
}

float2 RayTexelSize()
{
    return cRaySource.zw;
}

struct RayMask
{
    float4 rays : COLOR0;
    float4 openWithoutTheCharacter : COLOR1;
};

float2 SkyShareWithAndWithoutTheCharacter(float2 firstPixel, float2 pixelsPerDepthTap)
{
    float2 share = 0;
    [unroll] for (int y = 0; y < kDepthTapsPerSourceTexel; y++)
    {
        [unroll] for (int x = 0; x < kDepthTapsPerSourceTexel; x++)
        {
            float2 pixel = min(firstPixel + float2(x, y) * pixelsPerDepthTap, ViewportLastPixelCentre());
            float depth = SampleDepth(sDepth, pixel);
            bool sky = IsSky(depth);
            bool character = OnTheOwnCharacter(pixel, depth, cCharacterBox, cCharacterFocus, cCharacterHeights);
            share += float2(sky ? 1 : 0, sky || character ? 1 : 0);
        }
    }
    return share / (kDepthTapsPerSourceTexel * kDepthTapsPerSourceTexel);
}

float3 BrightSky(float3 scene, float skyShare)
{
    float luminance = dot(scene, kLumaWeights);
    return scene * skyShare * saturate((luminance - LuminanceThreshold()) * kThresholdRampSlope);
}

RayMask main(float2 rayTexel : VPOS)
{
    float2 fullPixelsPerRayTexel = ViewportSize() * RayTexelSize();
    float2 pixelsPerDepthTap = fullPixelsPerRayTexel / kDepthTapsPerRayTexel;
    float2 footprintOrigin = ViewportOrigin() + rayTexel * fullPixelsPerRayTexel;
    float2 centre = min(footprintOrigin + 0.5 * fullPixelsPerRayTexel, ViewportLastPixelCentre());
    float3 viewDirection = normalize(ViewRayAtUnitDepth(centre));
    float sunFalloff = pow(saturate(dot(viewDirection, DirectionToSunView())), AngularFalloffExponent());
    float3 brightSky = 0;
    float2 openSky = 0;
    [unroll] for (int y = 0; y < kSourceTexelsPerRayTexel; y++)
    {
        [unroll] for (int x = 0; x < kSourceTexelsPerRayTexel; x++)
        {
            float2 sourceTexel = float2(x, y);
            float2 sourceUv = (rayTexel + (sourceTexel + 0.5) / kSourceTexelsPerRayTexel) * RayTexelSize();
            float3 scene = tex2Dlod(sRaySource, float4(sourceUv, 0, 0)).rgb;
            float2 firstPixel = footprintOrigin + (sourceTexel * kDepthTapsPerSourceTexel + 0.5) * pixelsPerDepthTap;
            float2 skyShare = SkyShareWithAndWithoutTheCharacter(firstPixel, pixelsPerDepthTap);
            brightSky += BrightSky(scene, skyShare.x);
            openSky += skyShare;
        }
    }
    float sourceTexels = kSourceTexelsPerRayTexel * kSourceTexelsPerRayTexel;
    RayMask mask;
    mask.rays = float4(brightSky * sunFalloff / sourceTexels, openSky.x / sourceTexels);
    mask.openWithoutTheCharacter = openSky.y / sourceTexels;
    return mask;
}

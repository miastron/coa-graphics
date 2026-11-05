#include "vf_common.hlsli"

float4 cRayMask : register(c9);
float4 cRaySource : register(c10);

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

float SkyShare(float2 firstPixel, float2 pixelsPerDepthTap)
{
    float sky = 0;
    [unroll] for (int y = 0; y < kDepthTapsPerSourceTexel; y++)
    {
        [unroll] for (int x = 0; x < kDepthTapsPerSourceTexel; x++)
        {
            float2 pixel = min(firstPixel + float2(x, y) * pixelsPerDepthTap, ViewportLastPixelCentre());
            sky += IsSky(SampleDepth(sDepth, pixel)) ? 1 : 0;
        }
    }
    return sky / (kDepthTapsPerSourceTexel * kDepthTapsPerSourceTexel);
}

float3 BrightSky(float3 scene, float skyShare)
{
    float luminance = dot(scene, kLumaWeights);
    return scene * skyShare * saturate((luminance - LuminanceThreshold()) * kThresholdRampSlope);
}

float4 main(float2 rayTexel : VPOS) : COLOR0
{
    float2 fullPixelsPerRayTexel = ViewportSize() * RayTexelSize();
    float2 pixelsPerDepthTap = fullPixelsPerRayTexel / kDepthTapsPerRayTexel;
    float2 footprintOrigin = ViewportOrigin() + rayTexel * fullPixelsPerRayTexel;
    float2 centre = min(footprintOrigin + 0.5 * fullPixelsPerRayTexel, ViewportLastPixelCentre());
    float3 viewDirection = normalize(ViewRayAtUnitDepth(centre));
    float sunFalloff = pow(saturate(dot(viewDirection, DirectionToSunView())), AngularFalloffExponent());
    float3 brightSky = 0;
    float openSky = 0;
    [unroll] for (int y = 0; y < kSourceTexelsPerRayTexel; y++)
    {
        [unroll] for (int x = 0; x < kSourceTexelsPerRayTexel; x++)
        {
            float2 sourceTexel = float2(x, y);
            float2 sourceUv = (rayTexel + (sourceTexel + 0.5) / kSourceTexelsPerRayTexel) * RayTexelSize();
            float3 scene = tex2Dlod(sRaySource, float4(sourceUv, 0, 0)).rgb;
            float2 firstPixel = footprintOrigin + (sourceTexel * kDepthTapsPerSourceTexel + 0.5) * pixelsPerDepthTap;
            float skyShare = SkyShare(firstPixel, pixelsPerDepthTap);
            brightSky += BrightSky(scene, skyShare);
            openSky += skyShare;
        }
    }
    float sourceTexels = kSourceTexelsPerRayTexel * kSourceTexelsPerRayTexel;
    return float4(brightSky * sunFalloff / sourceTexels, openSky / sourceTexels);
}

#include "vf_composite.hlsli"

float4 main(float2 pixelIndex : VPOS) : COLOR0
{
    float2 pixel = pixelIndex + 0.5;
    float2 viewportUv = (pixel - ViewportOrigin()) / ViewportSize();
    float3 scene = tex2Dlod(sSceneBeforeFog, float4(viewportUv, 0, 0)).rgb;
    float depth = SampleDepth(sDepth, pixel);
    return float4(
        DitheredOverScene(scene, BlendGodRays(scene, DisplaySpaceGodRays(viewportUv, pixel, depth)), pixelIndex), 1);
}

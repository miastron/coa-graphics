#include "vf_common.hlsli"
#include "vf_local_light_data.hlsli"

sampler2D sDepth : register(s0);
sampler2D sSceneBeforeLights : register(s1);
sampler2D sCharacterShadow : register(s11);
sampler2D sNearShadow : register(s12);
sampler2D sMiddleShadow : register(s13);
sampler2D sFarShadow : register(s14);

float4 cViewToShadow[12] : register(c55);
float4 cSurfaceLight : register(c100);
float4 cSurfaceAmbient : register(c101);
float4 cSurfaceDirect : register(c102);
float4 cSurfaceToLight : register(c103);
float4 cClientShadows : register(c104);

static const float kDisplayGamma = 2.2;
static const float kDarkestIlluminant = 0.02;
static const float kFacingWrap = 0.25;
static const float kFadeSharpness = 4.0;
static const float kFaintestLight = 1.0 / 1024.0;
static const float kSmallestScene = 1e-6;
static const float kMaxEstimatedAlbedo = 0.5;
static const float kWhitestSurface = 1.0;
static const float kAlreadyLitSurface = 2.0;
static const float kShadowedGroundShare = 0.7;
static const float kSteepestGround = 0.5;
static const float kFlatGround = 0.8;
static const float2 kCharacterMapFade = float2(-3.4482758, 3.41379309);
static const float2 kFarMapFade = float2(-11.1111107, 11);
static const int kCharacterShadowMap = 0;
static const int kNearShadowMap = 1;
static const int kMiddleShadowMap = 2;
static const int kFarShadowMap = 3;
static const int kShadowTapsAroundCentre = 4;
static const float2 kShadowTaps[kShadowTapsAroundCentre] = {float2(0.8, -1.0), float2(0.2, -0.6),
                                                            float2(-0.6, -0.2), float2(-1.0, -0.4)};

float SurfaceLightStrength()
{
    return cSurfaceLight.x;
}

float SurfaceLightReach()
{
    return cSurfaceLight.y;
}

bool DrawsToSceneTexture()
{
    return cSurfaceLight.z > 0.5;
}

bool KeepsUnlitPixels()
{
    return cSurfaceLight.w > 0.5;
}

bool CharacterShadowMapPresent()
{
    return cClientShadows.x > 0.5;
}

bool EnvironmentShadowMapsPresent()
{
    return cClientShadows.y > 0.5;
}

bool ShadowMapsCompareInHardware()
{
    return cClientShadows.z > 0.5;
}

float ShadowMapTexel()
{
    return cClientShadows.w;
}

float3 ShadowPosition(float3 viewPosition, int mapIndex)
{
    float4 position = float4(viewPosition, 1);
    int firstRow = mapIndex * 3;
    return float3(dot(position, cViewToShadow[firstRow]), dot(position, cViewToShadow[firstRow + 1]),
                  dot(position, cViewToShadow[firstRow + 2]));
}

float ShadowMapEdge(float3 shadowPosition)
{
    return max(abs(shadowPosition.x), abs(shadowPosition.y));
}

float ShadowTapLit(sampler2D shadowMap, float2 uv, float depth)
{
    float stored = tex2Dlod(shadowMap, float4(uv, depth, 0)).r;
    return ShadowMapsCompareInHardware() ? stored : step(depth, stored);
}

float ShadowMapLit(sampler2D shadowMap, float3 shadowPosition)
{
    float2 uv = shadowPosition.xy * 0.5 + 0.5;
    float lit = ShadowTapLit(shadowMap, uv, shadowPosition.z);
    for (int tap = 0; tap < kShadowTapsAroundCentre; ++tap)
        lit += ShadowTapLit(shadowMap, uv + kShadowTaps[tap] * ShadowMapTexel(), shadowPosition.z);
    return lit / (kShadowTapsAroundCentre + 1);
}

float FadedShadowMapLit(sampler2D shadowMap, float3 shadowPosition, float2 fade)
{
    float coverage = saturate(ShadowMapEdge(shadowPosition) * fade.x + fade.y);
    [branch] if (coverage <= 0)
        return 1;
    return 1 + coverage * (ShadowMapLit(shadowMap, shadowPosition) - 1);
}

float EnvironmentShadowMapsLit(float3 viewPosition)
{
    float3 nearPosition = ShadowPosition(viewPosition, kNearShadowMap);
    [branch] if (ShadowMapEdge(nearPosition) < 1)
        return ShadowMapLit(sNearShadow, nearPosition);
    float3 middlePosition = ShadowPosition(viewPosition, kMiddleShadowMap);
    [branch] if (ShadowMapEdge(middlePosition) < 1)
        return ShadowMapLit(sMiddleShadow, middlePosition);
    return FadedShadowMapLit(sFarShadow, ShadowPosition(viewPosition, kFarShadowMap), kFarMapFade);
}

float ClientShadowMapsLit(float3 viewPosition)
{
    float lit = 1;
    [branch] if (CharacterShadowMapPresent())
        lit = FadedShadowMapLit(sCharacterShadow, ShadowPosition(viewPosition, kCharacterShadowMap),
                                kCharacterMapFade);
    [branch] if (EnvironmentShadowMapsPresent())
        lit = min(lit, EnvironmentShadowMapsLit(viewPosition));
    return lit;
}

float Ground(float3 normal)
{
    return smoothstep(kSteepestGround, kFlatGround, ViewToWorldDirection(normal).z);
}

float ClientShadowShade(float3 viewPosition, float ground)
{
    [branch] if (ground <= 0 || !CharacterShadowMapPresent())
        return 1;
    float share = lerp(kShadowedGroundShare, 1, ClientShadowMapsLit(viewPosition));
    return lerp(1, pow(max(share, kShadowedGroundShare), kDisplayGamma), ground);
}

float3 ViewPositionAt(float2 pixel)
{
    return ViewRayAtUnitDepth(pixel) * LinearDepth(SampleDepth(sDepth, pixel));
}

float3 NearerDifference(float3 centre, float3 before, float3 after)
{
    float3 backward = centre - before;
    float3 forward = after - centre;
    return abs(backward.z) < abs(forward.z) ? backward : forward;
}

float3 SurfaceNormal(float2 pixel, float3 centre)
{
    float3 across =
        NearerDifference(centre, ViewPositionAt(pixel - float2(1, 0)), ViewPositionAt(pixel + float2(1, 0)));
    float3 down = NearerDifference(centre, ViewPositionAt(pixel - float2(0, 1)), ViewPositionAt(pixel + float2(0, 1)));
    float3 normal = normalize(cross(across, down));
    return dot(normal, centre) > 0 ? -normal : normal;
}

float Facing(float3 normal, float3 direction)
{
    return saturate((dot(normal, direction) + kFacingWrap) / (1 + kFacingWrap));
}

float3 PointLightOnSurface(int index, float3 position, float3 normal)
{
    float4 positionRadius = LocalLightParameter(index * 3);
    float3 toLight = positionRadius.xyz - position;
    float distanceSquared = max(dot(toLight, toLight), 1e-6);
    [branch] if (distanceSquared >= positionRadius.w * positionRadius.w)
        return 0;
    float3 attenuation = LocalLightAttenuation(index);
    float inverseDistance = rsqrt(distanceSquared);
    float lightDistance = distanceSquared * inverseDistance;
    float falloff = rcp(max(attenuation.x + attenuation.y * lightDistance + attenuation.z * distanceSquared, 1));
    float boundaryFade = saturate((positionRadius.w - lightDistance) * kFadeSharpness / max(positionRadius.w, 1e-3));
    float3 colour = LocalLightColour(index) * LocalLightSurfaceStrength(index);
    return colour * falloff * boundaryFade * Facing(normal, toLight * inverseDistance);
}

float3 EstimatedAlbedo(float3 sceneLinear, float3 normal, float clientShadowShade, float ground)
{
    float3 illuminant = cSurfaceAmbient.rgb + cSurfaceDirect.rgb * saturate(dot(normal, cSurfaceToLight.xyz));
    float3 albedo = sceneLinear / (max(illuminant, kDarkestIlluminant) * clientShadowShade);
    float brightest = max(albedo.r, max(albedo.g, albedo.b));
    float explainedByTheSky = 1 - smoothstep(kWhitestSurface, kAlreadyLitSurface, brightest);
    return min(albedo, kMaxEstimatedAlbedo) * lerp(explainedByTheSky, 1, ground);
}

float4 Unlit(float4 scene)
{
    clip(KeepsUnlitPixels() ? 1 : -1);
    return scene;
}

float4 main(float2 pixelIndex : VPOS) : COLOR0
{
    float2 pixel = pixelIndex + 0.5 + (DrawsToSceneTexture() ? ViewportOrigin() : 0);
    float2 viewportUv = (pixel - ViewportOrigin()) / ViewportSize();
    float4 scene = tex2Dlod(sSceneBeforeLights, float4(viewportUv, 0, 0));
    float depth = SampleDepth(sDepth, pixel);
    if (BeyondFarClip(depth))
        return Unlit(scene);
    float3 position = ViewRayAtUnitDepth(pixel) * LinearDepth(depth);
    if (length(position) > SurfaceLightReach())
        return Unlit(scene);
    float3 normal = SurfaceNormal(pixel, position);
    float3 light = 0;
    [loop] for (int index = 0; index < LocalLightCount(); ++index)
        light += PointLightOnSurface(index, position, normal);
    if (max(light.r, max(light.g, light.b)) < kFaintestLight)
        return Unlit(scene);
    float3 sceneLinear = pow(max(scene.rgb, kSmallestScene), kDisplayGamma);
    float ground = Ground(normal);
    float3 albedo = EstimatedAlbedo(sceneLinear, normal, ClientShadowShade(position, ground), ground);
    float3 lit = sceneLinear + albedo * light * SurfaceLightStrength();
    return float4(pow(saturate(lit), 1 / kDisplayGamma), 1);
}

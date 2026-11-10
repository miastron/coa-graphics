#include "vf_integrate.hlsli"

float4 cComposite : register(c96);
float4 cGodRayColour : register(c97);
float4 cSun : register(c98);
float4 cCharacterBox : register(c100);
float4 cCharacterFocus : register(c101);
float4 cCharacterHeights : register(c102);

sampler2D sFog : register(s1);
sampler2D sGodRays : register(s2);
sampler2D sSceneBeforeFog : register(s3);
sampler2D sCurrentMarch : register(s4);

static const float kHighlightKnee = 0.8;
static const float kSunMarkerRadius = 6;
static const float4 kSunMarkerColour = float4(1, 0, 0, 1);
static const float kDebugRadiance = 1;
static const float kDebugTransmittance = 2;
static const float kDebugLinearDepth = 3;
static const float kSameSurfaceAbsoluteDepth = 0.5;
static const float kSameSurfaceRelativeDepth = 0.02;
static const float kTapPlaneRelativeDepth = 0.005;
static const float kWholeBilinearWeight = 0.999;
static const float kMaxDisplayedMarchCurvature = 2.0 / 255;
static const float kDisplayLevels = 255;
static const float kDitherRoundingGuard = 0.001;
static const float kInnerGlowPieceFraction = 0.3;
static const bool kCorrectsGlowBetweenTaps = kMarchesLocalLights && kTracesSunShadows && !kSamplesAuthoredNoise;

float Exposure()
{
    return cComposite.x;
}

float GodRayStrength()
{
    return cComposite.y;
}

float DebugView()
{
    return cComposite.z;
}

bool DebugViewEnabled()
{
    return DebugView() > 0.5;
}

bool DebugViewAtMost(float view)
{
    return DebugView() < view + 0.5;
}

float BlendMode()
{
    return cComposite.w;
}

bool BlendsInLinearLight()
{
    return BlendMode() > 0.5 && BlendMode() < 2.5;
}

bool BlendsOverSceneCopy()
{
    return (BlendMode() > 0.5 && BlendMode() < 1.5) || BlendMode() > 2.5;
}

float2 SunPixel()
{
    return cSun.xy;
}

bool SunMarkerEnabled()
{
    return cSun.z > 0;
}

float ClientGlowToCompensate()
{
    return cSun.w;
}

float3 GammaToLinear(float3 colour)
{
    return pow(colour, 2.2);
}

float3 LinearToGamma(float3 colour)
{
    return pow(colour, 1 / 2.2);
}

float3 RollOffHighlights(float3 colour, float3 knee)
{
    float3 span = max(1 - knee, 1e-4);
    return colour <= knee ? colour : knee + span * (1 - exp(-(colour - knee) / span));
}

float3 BeforeClientGlow(float3 onScreen, float glow)
{
    return 2 * onScreen / (1 + sqrt(1 + 4 * glow * onScreen));
}

bool SameDepthClass(float depth, float tapDepth)
{
    return IsSky(depth) == IsSky(tapDepth) && BeyondFarClip(depth) == BeyondFarClip(tapDepth);
}

struct BilinearTaps
{
    float2 baseTexel;
    float2 fraction;
    float4 currentMarch;
    float depth;
    float sameClassWeight;
    float farthestViewZ;
};

BilinearTaps GatherBilinearTaps(float2 pixel, float depth, float viewZ)
{
    float2 lowResCoord = FullPixelToLowResTexel(pixel);
    BilinearTaps taps;
    taps.baseTexel = floor(lowResCoord);
    taps.fraction = lowResCoord - taps.baseTexel;
    taps.currentMarch = 0;
    taps.depth = 0;
    taps.sameClassWeight = 0;
    taps.farthestViewZ = viewZ;
    [loop] for (int j = 0; j < 4; j++)
    {
        float2 tapOffset = float2(frac(j * 0.5) * 2, floor(j * 0.5));
        float2 tapTexel = clamp(taps.baseTexel + tapOffset, 0, LowResSize() - 1);
        float tapDepth = SampleDepth(sDepth, LowResTexelToFullPixel(tapTexel));
        float2 bilinearWeight = lerp(1 - taps.fraction, taps.fraction, tapOffset);
        float weight = bilinearWeight.x * bilinearWeight.y;
        taps.currentMarch += tex2Dlod(sCurrentMarch, float4(LowResTexelToUv(tapTexel), 0, 0)) * weight;
        taps.depth += tapDepth * weight;
        taps.sameClassWeight += SameDepthClass(depth, tapDepth) ? weight : 0;
        taps.farthestViewZ = max(taps.farthestViewZ, weight > 0 ? LinearDepth(tapDepth) : viewZ);
    }
    return taps;
}

bool PixelOnTapPlane(BilinearTaps taps, float viewZ)
{
    return taps.sameClassWeight > kWholeBilinearWeight && taps.farthestViewZ < HorizonBlendStart() &&
           abs(LinearDepth(taps.depth) - viewZ) <= viewZ * kTapPlaneRelativeDepth;
}

bool RayMeetsLocalLight(float2 pixel, float viewZ)
{
    float3 viewRay = ViewRayAtUnitDepth(pixel);
    float distancePerViewZ = length(viewRay);
    float3 viewDirection = viewRay / distancePerViewZ;
    float rayLength = viewZ * distancePerViewZ;
    bool meets = false;
    [loop] for (int index = 0; index < LocalLightCount(); ++index)
    {
        float2 chord = LoadLocalLight(index, viewDirection).chord;
        meets = meets || (chord.y > 0 && chord.x < rayLength);
    }
    return meets;
}

float4 DisplayedCurrentMarch(float2 lowResTexel)
{
    float2 uv = LowResTexelToUv(clamp(lowResTexel, 0, LowResSize() - 1));
    float4 fog = tex2Dlod(sCurrentMarch, float4(uv, 0, 0));
    fog.rgb *= Exposure();
    fog.rgb = BlendsInLinearLight() ? sqrt(max(fog.rgb, 0)) : fog.rgb;
    return fog;
}

float4 DisplayedMarchCurvature(float2 firstTexel, float2 axis)
{
    float4 before = DisplayedCurrentMarch(firstTexel - axis);
    float4 first = DisplayedCurrentMarch(firstTexel);
    float4 second = DisplayedCurrentMarch(firstTexel + axis);
    float4 after = DisplayedCurrentMarch(firstTexel + 2 * axis);
    return max(abs(before - 2 * first + second), abs(first - 2 * second + after));
}

bool MarchLinearBetweenTaps(BilinearTaps taps)
{
    float4 curvature = 0;
    [loop] for (int k = 0; k < 4; k++)
    {
        float2 axis = k < 2 ? float2(0, 1) : float2(1, 0);
        float2 across = 1 - axis;
        bool secondLine = k == 1 || k == 3;
        bool interpolated = dot(taps.fraction, axis) > 0 && (!secondLine || dot(taps.fraction, across) > 0);
        float2 firstTexel = taps.baseTexel + (secondLine ? across : float2(0, 0));
        [branch] if (interpolated)
            curvature = max(curvature, DisplayedMarchCurvature(firstTexel, axis));
    }
    return max(max(curvature.r, curvature.g), max(curvature.b, curvature.a)) <= kMaxDisplayedMarchCurvature;
}

bool TapsReproduceMarch(BilinearTaps taps, float viewZ)
{
    [branch] if (!PixelOnTapPlane(taps, viewZ))
        return false;
    return MarchLinearBetweenTaps(taps);
}

bool LitCompositeUnderNoise()
{
    return kMarchesLocalLights && AnyLayerNoise();
}

bool LightMeetsRayOutsideNoise(float2 pixel, float viewZ)
{
    [branch] if (!kMarchesLocalLights || LitCompositeUnderNoise())
        return false;
    return RayMeetsLocalLight(pixel, viewZ);
}

float LitDensityNearLight(float distanceAlongRay, float height)
{
    float density = 0;
    [unroll] for (int j = 0; j < kFogLayers; j++)
    {
        FogLayer layer = LoadConstantFogLayer(j);
        bool lit = layer.density > 0 && distanceAlongRay >= layer.start && distanceAlongRay < layer.limit &&
                   (LocalLightReachLimit() <= 0 || distanceAlongRay < LocalLightReachLimit());
        density += lit ? layer.density * DistanceCurve(layer, distanceAlongRay) * HeightProfile(layer, height) : 0;
    }
    return density;
}

float GaussGlowOverInterval(LocalLight light, float3 attenuation, float3 viewDirection, float2 interval,
                            float density, float extinctionPerYard)
{
    float scattering = 0;
    [unroll] for (int sample = 0; sample < 2; ++sample)
    {
        float along = lerp(interval.x, interval.y, QuadratureFraction(sample));
        scattering += LightSampleScattering(light, attenuation, viewDirection, along, density) *
                      exp(-extinctionPerYard * along);
    }
    return 0.5 * scattering * (interval.y - interval.x);
}

float GlowAroundNearestPoint(LocalLight light, float3 attenuation, float3 viewDirection, float2 chord, float nearest,
                             float density, float extinctionPerYard)
{
    float2 inner = lerp(nearest, chord, kInnerGlowPieceFraction);
    return GaussGlowOverInterval(light, attenuation, viewDirection, float2(chord.x, inner.x), density,
                                 extinctionPerYard) +
           GaussGlowOverInterval(light, attenuation, viewDirection, float2(inner.x, nearest), density,
                                 extinctionPerYard) +
           GaussGlowOverInterval(light, attenuation, viewDirection, float2(nearest, inner.y), density,
                                 extinctionPerYard) +
           GaussGlowOverInterval(light, attenuation, viewDirection, float2(inner.y, chord.y), density,
                                 extinctionPerYard);
}

float3 LightGlowAlongRay(float2 pixel, float viewZ, float transmittance)
{
    float3 viewRay = ViewRayAtUnitDepth(pixel);
    float distancePerViewZ = length(viewRay);
    float3 viewDirection = viewRay / distancePerViewZ;
    float rayLength = min(viewZ * distancePerViewZ, MaxFogDistance());
    float heightPerYard = ViewToWorldDirection(viewDirection).z;
    float extinctionPerYard = -log(max(transmittance, kOpaqueTransmittanceFloor)) / max(rayLength, 1e-3);
    float3 glow = 0;
    [loop] for (int index = 0; index < LocalLightCount(); ++index)
    {
        LocalLight light = LoadLocalLight(index, viewDirection);
        float2 chord = float2(max(light.chord.x, 0), min(light.chord.y, rayLength));
        [branch] if (chord.y > chord.x)
        {
            float nearest = clamp(light.projected, chord.x, chord.y);
            float density = LitDensityNearLight(nearest, CameraPositionWorld().z + heightPerYard * nearest);
            glow += LocalLightFogColour(index) * GlowAroundNearestPoint(light, LocalLightAttenuation(index),
                                                                        viewDirection, chord, nearest, density,
                                                                        extinctionPerYard);
        }
    }
    return glow;
}

float4 GlowCorrectedTaps(float2 pixel, float viewZ, BilinearTaps taps)
{
    float3 glow = LightGlowAlongRay(pixel, viewZ, 1 - taps.currentMarch.a);
    [loop] for (int j = 0; j < 4; j++)
    {
        float2 tapOffset = float2(frac(j * 0.5) * 2, floor(j * 0.5));
        float2 bilinearWeight = lerp(1 - taps.fraction, taps.fraction, tapOffset);
        float weight = bilinearWeight.x * bilinearWeight.y;
        [branch] if (weight > 0)
        {
            float2 tapTexel = clamp(taps.baseTexel + tapOffset, 0, LowResSize() - 1);
            float2 tapPixel = LowResTexelToFullPixel(tapTexel);
            float tapViewZ = LinearDepth(SampleDepth(sDepth, tapPixel));
            float tapOpacity = tex2Dlod(sCurrentMarch, float4(LowResTexelToUv(tapTexel), 0, 0)).a;
            glow -= weight * LightGlowAlongRay(tapPixel, tapViewZ, 1 - tapOpacity);
        }
    }
    return float4(max(taps.currentMarch.rgb + glow, 0), taps.currentMarch.a);
}

float4 FilteredFogBetweenTaps(BilinearTaps taps)
{
    float4 fog = 0;
    [loop] for (int j = 0; j < 4; j++)
    {
        float2 tapOffset = float2(frac(j * 0.5) * 2, floor(j * 0.5));
        float2 tapTexel = clamp(taps.baseTexel + tapOffset, 0, LowResSize() - 1);
        float2 bilinearWeight = lerp(1 - taps.fraction, taps.fraction, tapOffset);
        fog += tex2Dlod(sFog, float4(LowResTexelToUv(tapTexel), 0, 0)) * bilinearWeight.x * bilinearWeight.y;
    }
    return fog;
}

float4 FogWithoutMatchingTap(float2 pixel, float depth, float viewZ)
{
    bool lightOnRay = LightMeetsRayOutsideNoise(pixel, viewZ);
    [branch] if (!lightOnRay || kCorrectsGlowBetweenTaps)
    {
        BilinearTaps taps = GatherBilinearTaps(pixel, depth, viewZ);
        [branch] if (LitCompositeUnderNoise())
        {
            [branch] if (PixelOnTapPlane(taps, viewZ))
                return FilteredFogBetweenTaps(taps);
        }
        else [branch] if (kCorrectsGlowBetweenTaps && lightOnRay)
        {
            [branch] if (PixelOnTapPlane(taps, viewZ))
                return GlowCorrectedTaps(pixel, viewZ, taps);
        }
        else
        {
            [branch] if (TapsReproduceMarch(taps, viewZ))
                return taps.currentMarch;
        }
    }
    return IntegrateFogAtDepth(pixel, depth, 0.5);
}

float4 DepthAwareUpsample(float2 pixel, float depth, float viewZ)
{
    float2 lowResCoord = FullPixelToLowResTexel(pixel);
    float2 baseTexel = floor(lowResCoord);
    float2 bilinearFraction = lowResCoord - baseTexel;

    float4 weightedFog = 0;
    float weightSum = 0;
    [loop] for (int j = 0; j < 4; j++)
    {
        float2 tapOffset = float2(frac(j * 0.5) * 2, floor(j * 0.5));
        float2 tapTexel = clamp(baseTexel + tapOffset, 0, LowResSize() - 1);
        float tapDepth = SampleDepth(sDepth, LowResTexelToFullPixel(tapTexel));
        float tapViewZ = LinearDepth(tapDepth);
        float2 bilinearWeight = lerp(1 - bilinearFraction, bilinearFraction, tapOffset);
        bool sameClass = SameDepthClass(depth, tapDepth);
        bool sameDepth = abs(tapViewZ - viewZ) <= max(kSameSurfaceAbsoluteDepth, viewZ * kSameSurfaceRelativeDepth);
        float relativeDepthDifference = abs(tapViewZ - viewZ) / max(viewZ, 1e-3);
        float weight = sameClass && sameDepth ?
            bilinearWeight.x * bilinearWeight.y / (1e-3 + relativeDepthDifference) : 0;
        weightedFog += tex2Dlod(sFog, float4(LowResTexelToUv(tapTexel), 0, 0)) * weight;
        weightSum += weight;
    }
    [branch] if (weightSum > 1e-6)
        return weightedFog / weightSum;
    return FogWithoutMatchingTap(pixel, depth, viewZ);
}

float3 DisplaySpaceGodRays(float2 viewportUv, float2 pixel, float depth)
{
    float3 godRays = 0;
    [branch] if (GodRayStrength() > 0)
    {
        [branch] if (!OnTheOwnCharacter(pixel, depth, cCharacterBox, cCharacterFocus, cCharacterHeights))
            godRays = tex2Dlod(sGodRays, float4(viewportUv, 0, 0)).rgb * cGodRayColour.rgb * GodRayStrength();
    }
    return godRays;
}

float3 BlendGodRays(float3 colour, float3 godRays)
{
    [branch] if (GodRayStrength() > 0)
    {
        float glow = ClientGlowToCompensate();
        float3 displayed = colour * (1 + glow * colour);
        displayed += max(1 - displayed, 0) * (1 - exp(-max(godRays, 0)));
        colour = BeforeClientGlow(displayed, glow);
    }
    return colour;
}

float3 DitheredOverScene(float3 scene, float3 composited, float2 pixel)
{
    float guardedNoise = kDitherRoundingGuard + InterleavedGradientNoise(pixel) * (1 - 2 * kDitherRoundingGuard);
    return scene + floor((composited - scene) * kDisplayLevels + guardedNoise) / kDisplayLevels;
}

float3 FogSceneColour(float3 scene, float4 fog)
{
    [branch] if (!BlendsInLinearLight())
    {
        float3 fogColour = RollOffHighlights(fog.rgb / max(fog.a, 1e-4), kHighlightKnee);
        float3 colour = scene * (1 - fog.a) + fogColour * fog.a;
        return colour;
    }
    scene = GammaToLinear(scene);
    float3 colour = LinearToGamma(RollOffHighlights(scene * (1 - fog.a) + fog.rgb, max(kHighlightKnee, scene)));
    [branch] if (ClientGlowToCompensate() > 0)
        colour = lerp(colour, BeforeClientGlow(colour, ClientGlowToCompensate()), fog.a);
    return colour;
}

float4 BlendOverSceneCopy(float2 pixel, float2 viewportUv, float4 fog, float3 godRays)
{
    float3 scene = tex2Dlod(sSceneBeforeFog, float4(viewportUv, 0, 0)).rgb;
    return float4(DitheredOverScene(scene, BlendGodRays(FogSceneColour(scene, fog), godRays), pixel), 1);
}

float4 PremultipliedForFixedFunctionBlend(float2 pixel, float4 fog, float3 godRays, bool linearLight)
{
    float opacity = max(fog.a, 1e-4);
    float3 unpremultiplied = RollOffHighlights(fog.rgb / opacity, kHighlightKnee);
    if (linearLight)
        unpremultiplied = LinearToGamma(unpremultiplied);
    return float4(DitheredForDisplay(unpremultiplied * fog.a + godRays, pixel), fog.a);
}

float4 CompositeAtDepth(float2 pixel, float depth)
{
    float viewZ = LinearDepth(depth);
    float4 fog = DepthAwareUpsample(pixel, depth, viewZ);
    fog.rgb *= Exposure();

    float2 viewportUv = (pixel - ViewportOrigin()) / ViewportSize();
    float3 godRays = DisplaySpaceGodRays(viewportUv, pixel, depth);
    const bool linearLight = BlendsInLinearLight();

    [branch] if (DebugViewEnabled())
    {
        if (DebugViewAtMost(kDebugRadiance))
            return float4((linearLight ? LinearToGamma(saturate(fog.rgb)) : fog.rgb) + godRays, 1);
        if (DebugViewAtMost(kDebugTransmittance))
            return float4(1 - fog.aaa, 1);
        if (DebugViewAtMost(kDebugLinearDepth))
            return float4(saturate(viewZ / MaxFogDistance()).xxx, 1);
    }
    [branch] if (SunMarkerEnabled() && distance(pixel, SunPixel()) < kSunMarkerRadius)
        return kSunMarkerColour;

    [branch] if (BlendsOverSceneCopy())
        return BlendOverSceneCopy(pixel, viewportUv, fog, godRays);
    return PremultipliedForFixedFunctionBlend(pixel, fog, godRays, linearLight);
}

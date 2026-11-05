float4 cShaftArc : register(c9);
float4 cRayTarget : register(c10);

sampler2D sRays : register(s0);
sampler2D sOpenWithoutTheCharacter : register(s1);

static const int kArcSamplesPerSide = 12;
static const float kArcStepRadians = 0.044;
static const float kLeastOpenSky = 1.0 / 255;

float2 SunTexel()
{
    return cShaftArc.xy;
}

float2 TargetTexelSize()
{
    return cRayTarget.zw;
}

float4 RaysAt(float2 texel)
{
    return tex2Dlod(sRays, float4(texel * TargetTexelSize(), 0, 0));
}

float OpenSkyWithoutTheCharacterAt(float2 texel)
{
    return tex2Dlod(sOpenWithoutTheCharacter, float4(texel * TargetTexelSize(), 0, 0)).a;
}

float2 RotatedAboutTheSun(float2 fromSun, float angle)
{
    float s;
    float c;
    sincos(angle, s, c);
    return SunTexel() + float2(fromSun.x * c - fromSun.y * s, fromSun.x * s + fromSun.y * c);
}

float LeastOpenSkyOnOneSide(float2 fromSun, float turn)
{
    float least = OpenSkyWithoutTheCharacterAt(RotatedAboutTheSun(fromSun, turn));
    [unroll] for (int k = 2; k <= kArcSamplesPerSide; k++)
        least = min(least, OpenSkyWithoutTheCharacterAt(RotatedAboutTheSun(fromSun, k * turn)));
    return least;
}

float4 main(float2 rayTexel : VPOS) : COLOR0
{
    float2 texel = rayTexel + 0.5;
    float2 fromSun = texel - SunTexel();
    float4 rays = RaysAt(texel);
    float shadowEitherSide =
        max(LeastOpenSkyOnOneSide(fromSun, kArcStepRadians), LeastOpenSkyOnOneSide(fromSun, -kArcStepRadians));
    float shaftShare = saturate((rays.a - shadowEitherSide) / max(rays.a, kLeastOpenSky));
    return float4(rays.rgb * shaftShare, 1);
}

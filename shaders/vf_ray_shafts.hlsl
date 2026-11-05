float4 cShaftArc : register(c9);
float4 cRayTarget : register(c10);

sampler2D sRays : register(s0);

static const int kArcSamplesPerSide = 12;
static const float kArcStepRadians = 0.044;

float2 SunTexel()
{
    return cShaftArc.xy;
}

float2 TargetTexelSize()
{
    return cRayTarget.zw;
}

float3 RaysAt(float2 texel)
{
    return tex2Dlod(sRays, float4(texel * TargetTexelSize(), 0, 0)).rgb;
}

float2 RotatedAboutTheSun(float2 fromSun, float angle)
{
    float s;
    float c;
    sincos(angle, s, c);
    return SunTexel() + float2(fromSun.x * c - fromSun.y * s, fromSun.x * s + fromSun.y * c);
}

float3 DimmestOnOneSide(float2 fromSun, float turn)
{
    float3 dimmest = RaysAt(RotatedAboutTheSun(fromSun, turn));
    [unroll] for (int k = 2; k <= kArcSamplesPerSide; k++)
        dimmest = min(dimmest, RaysAt(RotatedAboutTheSun(fromSun, k * turn)));
    return dimmest;
}

float4 main(float2 rayTexel : VPOS) : COLOR0
{
    float2 texel = rayTexel + 0.5;
    float2 fromSun = texel - SunTexel();
    float3 rays = RaysAt(texel);
    float3 shadowEitherSide =
        max(DimmestOnOneSide(fromSun, kArcStepRadians), DimmestOnOneSide(fromSun, -kArcStepRadians));
    return float4(max(rays - shadowEitherSide, 0), 1);
}

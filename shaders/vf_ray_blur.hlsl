float4 cRadialBlur : register(c9);
float4 cRayTarget : register(c10);
float4 cRayDecay : register(c11);

sampler2D sSource : register(s0);

static const int kTaps = 8;

float2 SunUv()
{
    return cRadialBlur.xy;
}

float StepFractionTowardSun()
{
    return cRadialBlur.z;
}

float TapWeightNormalisation()
{
    return cRadialBlur.w;
}

float2 TargetTexelSize()
{
    return cRayTarget.zw;
}

float DecayPerTap()
{
    return cRayDecay.x;
}

float4 main(float2 texel : VPOS) : COLOR0
{
    float2 uv = (texel + 0.5) * TargetTexelSize();
    float2 tapStep = (SunUv() - uv) * StepFractionTowardSun();
    float4 sum = 0;
    float tapWeight = 1;
    [unroll] for (int k = 0; k < kTaps; k++)
    {
        sum += tex2Dlod(sSource, float4(uv + tapStep * k, 0, 0)) * tapWeight;
        tapWeight *= DecayPerTap();
    }
    return sum * TapWeightNormalisation();
}

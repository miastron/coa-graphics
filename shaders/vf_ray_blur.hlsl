float4 cRadialBlur : register(c9);
float4 cRayTarget : register(c10);
float4 cRayDecay : register(c11);

sampler2D sSource : register(s0);
sampler2D sOpenWithoutTheCharacter : register(s1);

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

struct BlurredRays
{
    float4 rays : COLOR0;
    float4 openWithoutTheCharacter : COLOR1;
};

BlurredRays main(float2 texel : VPOS)
{
    float2 uv = (texel + 0.5) * TargetTexelSize();
    float2 tapStep = (SunUv() - uv) * StepFractionTowardSun();
    float4 sum = 0;
    float openWithoutTheCharacter = 0;
    float tapWeight = 1;
    [unroll] for (int k = 0; k < kTaps; k++)
    {
        float4 tapUv = float4(uv + tapStep * k, 0, 0);
        sum += tex2Dlod(sSource, tapUv) * tapWeight;
        openWithoutTheCharacter += tex2Dlod(sOpenWithoutTheCharacter, tapUv).a * tapWeight;
        tapWeight *= DecayPerTap();
    }
    BlurredRays blurred;
    blurred.rays = sum * TapWeightNormalisation();
    blurred.openWithoutTheCharacter = openWithoutTheCharacter * TapWeightNormalisation();
    return blurred;
}

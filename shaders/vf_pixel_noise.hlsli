static const float kDisplayCodeMax = 255;
static const float kDitherCodeSpan = 0.99;

float InterleavedGradientNoise(float2 pixel)
{
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

float3 DitheredForDisplay(float3 colour, float2 pixel)
{
    return colour + (InterleavedGradientNoise(pixel) - 0.5) * (kDitherCodeSpan / kDisplayCodeMax);
}

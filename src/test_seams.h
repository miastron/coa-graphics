#pragma once

#ifdef VF_TESTING
inline constexpr bool kTestSeams = true;
#else
inline constexpr bool kTestSeams = false;
#endif

template <typename T>
constexpr T Seam(T testValue, T productionValue)
{
    return kTestSeams ? testValue : productionValue;
}

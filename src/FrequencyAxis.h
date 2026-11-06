#pragma once

#include <algorithm>
#include <cmath>

//==============================================================================
// Asse logaritmico 20 Hz - 20 kHz dell'analizzatore di spettro: posizione in
// pixel di una frequenza e viceversa (fuori vista si ferma ai bordi).
struct FrequencyAxis
{
    float width = 1.0f;

    static constexpr float minHz = 20.0f;
    static constexpr float maxHz = 20000.0f;

    float xForFrequency (float hz) const noexcept
    {
        return width * std::log (hz / minHz) / std::log (maxHz / minHz);
    }

    float frequencyForX (float x) const noexcept
    {
        const float t = std::clamp (x / width, 0.0f, 1.0f);
        return minHz * std::pow (maxHz / minHz, t);
    }
};

#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

//==============================================================================
// Matematica dell'accordatore, separata dalla grafica.
namespace pitch
{
    // YIN (difference function + CMND + interpolazione parabolica) su una finestra
    // decimata; tarato sul registro del basso (fino a ~400 Hz, low B a 31 Hz).
    // Restituisce la frequenza in Hz, o -1 se non c'è una nota riconoscibile.
    inline float detectYin (const float* window, int size, double sampleRate)
    {
        const int maxTau = size / 2;
        const int minTau = juce::jmax (2, (int) (sampleRate / 400.0));

        std::vector<float> diff ((size_t) maxTau, 0.0f);

        for (int tau = minTau; tau < maxTau; ++tau)
        {
            float sum = 0.0f;
            for (int i = 0; i < maxTau; ++i)
            {
                const float d = window[i] - window[i + tau];
                sum += d * d;
            }
            diff[(size_t) tau] = sum;
        }

        std::vector<float> cmnd ((size_t) maxTau, 1.0f);
        float runningSum = 0.0f;
        for (int tau = minTau; tau < maxTau; ++tau)
        {
            runningSum += diff[(size_t) tau];
            cmnd[(size_t) tau] = runningSum > 0.0f
                               ? diff[(size_t) tau] * (float) (tau - minTau + 1) / runningSum
                               : 1.0f;
        }

        constexpr float threshold = 0.15f;
        int tauEstimate = -1;
        for (int tau = minTau + 1; tau < maxTau - 1; ++tau)
        {
            if (cmnd[(size_t) tau] < threshold)
            {
                while (tau + 1 < maxTau - 1 && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau])
                    ++tau;
                tauEstimate = tau;
                break;
            }
        }

        if (tauEstimate < 0)
            return -1.0f;

        const float y0 = cmnd[(size_t) (tauEstimate - 1)];
        const float y1 = cmnd[(size_t) tauEstimate];
        const float y2 = cmnd[(size_t) (tauEstimate + 1)];
        const float denom = 2.0f * (y0 - 2.0f * y1 + y2);
        const float delta = std::abs (denom) > 1.0e-12f ? (y0 - y2) / denom : 0.0f;

        return (float) (sampleRate / ((double) tauEstimate + (double) delta));
    }

    struct Note
    {
        juce::String name;   // es. "A2", "C#3"
        float cents = 0.0f;  // -50..+50 rispetto alla nota più vicina
    };

    inline Note noteFor (float hz)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                       "F#", "G", "G#", "A", "A#", "B" };

        const float midi    = 69.0f + 12.0f * std::log2 (hz / 440.0f);
        const int   nearest = (int) std::lround (midi);
        const int   octave  = nearest / 12 - 1;
        return { juce::String (names[((nearest % 12) + 12) % 12]) + juce::String (octave),
                 (midi - (float) nearest) * 100.0f };
    }

    // Testo sotto la nota: "83.4 Hz  +20 cent" (cent interi: prima comparivano
    // con quattro decimali, es. "+19.8639 cent")
    inline juce::String readout (float hz)
    {
        const int cents = juce::roundToInt (noteFor (hz).cents);
        return juce::String (hz, 1) + " Hz  " + (cents >= 0 ? "+" : "") + juce::String (cents) + " cent";
    }
}

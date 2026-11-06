#pragma once

#include "JuceHeader.h"

/** Sample rate conversion of a continuous stream, fed in pieces of any length: every call
    returns the output samples whose positions are covered by the input so far (4-point
    Lagrange interpolation). Used around models that run at their own sample rate.
    No allocation after prepare(). */
class StreamingResampler {
public:
    void prepare(double inputRate, double outputRate, int maxInputPerCall) {
        step = inputRate / outputRate;
        history.assign((size_t) maxInputPerCall + 8, 0.f);
        reset();
    }

    void reset() {
        std::fill(history.begin(), history.end(), 0.f);
        count = 3;          // three samples of silence before the stream
        position = 1.0;
    }

    /** the most output samples a call with numInput samples can produce */
    int maxOutput(int numInput) const { return (int) std::ceil((numInput + 4) / step) + 2; }

    int process(const float* input, int numInput, float* output, int maxOut) {
        jassert(count + numInput <= (int) history.size());
        std::copy_n(input, numInput, history.begin() + count);
        count += numInput;

        int produced = 0;
        while (produced < maxOut) {
            const int i = (int) position;
            if (i + 2 >= count) break;
            const float t = (float) (position - i);
            const float ym1 = history[(size_t) i - 1], y0 = history[(size_t) i],
                        y1 = history[(size_t) i + 1], y2 = history[(size_t) i + 2];
            // 4-point, 3rd order Lagrange
            const float c0 = y0;
            const float c1 = y1 - ym1 / 3.f - y0 / 2.f - y2 / 6.f;
            const float c2 = (ym1 + y1) / 2.f - y0;
            const float c3 = (y2 - ym1) / 6.f + (y0 - y1) / 2.f;
            output[produced++] = ((c3 * t + c2) * t + c1) * t + c0;
            position += step;
        }

        // keep what the next output samples still need
        const int drop = juce::jmax(0, (int) position - 1);
        if (drop > 0) {
            std::copy(history.begin() + drop, history.begin() + count, history.begin());
            count -= drop;
            position -= drop;
        }
        return produced;
    }

private:
    double step = 1.0, position = 1.0;
    std::vector<float> history;
    int count = 3;
};

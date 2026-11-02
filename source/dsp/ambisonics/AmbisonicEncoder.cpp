//
// Ambisonic (AmbiX: ACN channel order, SN3D normalisation) point source encoder.
//

#include "AmbisonicEncoder.h"

void AmbisonicEncoder::computeCoefficients(float x, float y, float z, float spread, Coefficients &c) {
    // real spherical harmonics, ACN order, SN3D normalised
    static const float sqrt3 = std::sqrt(3.f);
    static const float sqrt15 = std::sqrt(15.f);
    static const float sqrt5_8 = std::sqrt(5.f / 8.f);
    static const float sqrt3_8 = std::sqrt(3.f / 8.f);

    const float x2 = x * x, y2 = y * y, z2 = z * z;

    c[0] = 1.f;

    c[1] = y;
    c[2] = z;
    c[3] = x;

    c[4] = sqrt3 * x * y;
    c[5] = sqrt3 * y * z;
    c[6] = 0.5f * (3.f * z2 - 1.f);
    c[7] = sqrt3 * x * z;
    c[8] = 0.5f * sqrt3 * (x2 - y2);

    c[9] = sqrt5_8 * y * (3.f * x2 - y2);
    c[10] = sqrt15 * x * y * z;
    c[11] = sqrt3_8 * y * (5.f * z2 - 1.f);
    c[12] = 0.5f * z * (5.f * z2 - 3.f);
    c[13] = sqrt3_8 * x * (5.f * z2 - 1.f);
    c[14] = 0.5f * sqrt15 * z * (x2 - y2);
    c[15] = sqrt5_8 * x * (x2 - 3.f * y2);

    // spread fades out the higher orders, at full spread only the omnidirectional W remains
    const float orderWeight = 1.f - juce::jlimit(0.f, 1.f, spread);
    float weight = 1.f;
    for (int n = 1; n <= maxOrder; ++n) {
        weight *= orderWeight;
        for (int acn = n * n; acn < (n + 1) * (n + 1); ++acn)
            c[(size_t) acn] *= weight;
    }
}

void AmbisonicEncoder::computeCoefficients(float azimuthDegrees, float elevationDegrees, float spread, Coefficients &c) {
    const float azimuth = juce::degreesToRadians(azimuthDegrees);
    const float elevation = juce::degreesToRadians(elevationDegrees);
    computeCoefficients(std::cos(elevation) * std::cos(azimuth),
                        std::cos(elevation) * std::sin(azimuth),
                        std::sin(elevation),
                        spread, c);
}

void AmbisonicEncoder::prepare(int newOrder) {
    order = juce::jlimit(0, maxOrder, newOrder);
    numChannels = getNumChannelsForOrder(order);
    reset();
}

void AmbisonicEncoder::reset() {
    current.fill(0.f);
    target.fill(0.f);
    firstBlock = true;
}

void AmbisonicEncoder::setTarget(const Coefficients &newCoefficients, float gain) {
    for (size_t i = 0; i < (size_t) maxChannels; ++i)
        target[i] = newCoefficients[i] * gain;

    if (firstBlock) {
        current = target;
        firstBlock = false;
    }
}

void AmbisonicEncoder::encodeAdding(const float *mono, juce::AudioBuffer<float> &output, int numSamples) {
    const int channels = juce::jmin(numChannels, output.getNumChannels());
    const float rampStep = numSamples > 0 ? 1.f / (float) numSamples : 0.f;

    for (int ch = 0; ch < channels; ++ch) {
        auto* write = output.getWritePointer(ch);
        const float start = current[(size_t) ch];
        const float delta = (target[(size_t) ch] - start) * rampStep;

        if (delta == 0.f) {
            juce::FloatVectorOperations::addWithMultiply(write, mono, start, numSamples);
        } else {
            for (int i = 0; i < numSamples; ++i)
                write[i] += mono[i] * (start + delta * (float) (i + 1));
        }
    }
    current = target;
}

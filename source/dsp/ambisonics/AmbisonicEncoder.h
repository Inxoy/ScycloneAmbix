//
// Ambisonic (AmbiX: ACN channel order, SN3D normalisation) point source encoder.
//

#ifndef SCYCLONE_AMBISONICENCODER_H
#define SCYCLONE_AMBISONICENCODER_H

#include <JuceHeader.h>
#include <array>

class AmbisonicEncoder {
public:
    static constexpr int maxOrder = 3;
    static constexpr int maxChannels = (maxOrder + 1) * (maxOrder + 1);
    using Coefficients = std::array<float, maxChannels>;

    static int getNumChannelsForOrder(int order) { return (order + 1) * (order + 1); }
    // 4, 9 and 16 channels are first to third order, anything else is no ambisonic signal (-1)
    static int getOrderForNumChannels(int numChannels) {
        for (int order = 1; order <= maxOrder; ++order)
            if (getNumChannelsForOrder(order) == numChannels) return order;
        return -1;
    }

    // direction is a unit vector (x front, y left, z up), spread 0 = point source, 1 = omni
    static void computeCoefficients(float x, float y, float z, float spread, Coefficients& coefficients);
    static void computeCoefficients(float azimuthDegrees, float elevationDegrees, float spread, Coefficients& coefficients);

    void prepare(int newOrder);
    void reset();
    void setTarget(const Coefficients& newCoefficients, float gain);

    // adds the encoded mono signal to the first channels of output, ramping from the
    // previous to the current target over the block
    void encodeAdding(const float* mono, juce::AudioBuffer<float>& output, int numSamples);

private:
    int order = 1;
    int numChannels = 4;
    bool firstBlock = true;
    Coefficients current {};
    Coefficients target {};
};

#endif //SCYCLONE_AMBISONICENCODER_H

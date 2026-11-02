//
// Encodes the two network outputs as AmbiX point sources whose azimuth, elevation and
// spread are modulated by the networks' latent space.
//

#ifndef SCYCLONE_PROCESSORAMBISONICS_H
#define SCYCLONE_PROCESSORAMBISONICS_H

#include <JuceHeader.h>
#include "AmbisonicEncoder.h"
#include "../../PluginParameters.h"

// tracks running mean and variance per latent dimension and maps values to [-1, 1]
class LatentNormaliser {
public:
    void reset();
    void process(const std::vector<float>& latent, int numDimensions, float blockDurationSeconds);
    float getNormalised(int dimension) const;

private:
    static constexpr float adaptationTimeSeconds = 8.f;
    std::vector<float> mean, variance, normalised;
    float observedSeconds = 0.f;
};

class ProcessorAmbisonics {
public:
    explicit ProcessorAmbisonics(juce::AudioProcessorValueTreeState& apvts);

    void prepare(double sampleRate, int order);

    // adds network number (1 or 2) to output, gain is applied on top of the encoding
    void encodeNetwork(int network, const juce::AudioBuffer<float>& mono, const std::vector<float>& latent,
                       int numLatentDimensions, float gain, juce::AudioBuffer<float>& output);
    // writes the (mono or stereo) input as a source pair at +/- dry width into output
    void encodeDry(const juce::AudioBuffer<float>& input, int numInputChannels, juce::AudioBuffer<float>& output);

    float getCurrentAzimuth(int network) const;
    float getCurrentElevation(int network) const;

private:
    struct SourceParameters {
        std::atomic<float>* azimuth;
        std::atomic<float>* elevation;
        std::atomic<float>* spread;
        std::atomic<float>* azimuthDepth;
        std::atomic<float>* elevationDepth;
        std::atomic<float>* spreadDepth;
        std::atomic<float>* azimuthLatent;
        std::atomic<float>* elevationLatent;
        std::atomic<float>* spreadLatent;
    };

    struct LatentSource {
        SourceParameters parameters;
        LatentNormaliser normaliser;
        AmbisonicEncoder encoder;
        juce::Vector3D<float> direction {1.f, 0.f, 0.f};
        float spread = 0.f;
        bool initialised = false;
        std::atomic<float> currentAzimuth {0.f};
        std::atomic<float> currentElevation {0.f};
    };

    float getSmoothingCoefficient(int numSamples) const;

    std::atomic<float>* smoothingTime;
    std::atomic<float>* dryWidth;

    std::array<LatentSource, 2> sources;
    AmbisonicEncoder dryEncoderLeft, dryEncoderRight;
    double sampleRate = 48000.0;
};

#endif //SCYCLONE_PROCESSORAMBISONICS_H

//
// Encodes the two network outputs as AmbiX point sources whose azimuth, elevation and
// spread are modulated by the networks' latent space.
//

#include "ProcessorAmbisonics.h"

void LatentNormaliser::reset() {
    mean.clear();
    variance.clear();
    normalised.clear();
    observedSeconds = 0.f;
}

void LatentNormaliser::process(const std::vector<float> &latent, int numDimensions, float blockDurationSeconds) {
    if ((int) mean.size() != numDimensions) {
        mean.assign((size_t) numDimensions, 0.f);
        variance.assign((size_t) numDimensions, 0.f);
        normalised.assign((size_t) numDimensions, 0.f);
        observedSeconds = 0.f;
    }
    if (numDimensions == 0) return;

    // behaves like a cumulative average until the adaptation time is reached, so the
    // statistics settle quickly after a model was loaded
    observedSeconds += blockDurationSeconds;
    const float alpha = blockDurationSeconds / juce::jmin(observedSeconds, adaptationTimeSeconds);

    for (size_t dim = 0; dim < (size_t) numDimensions; ++dim) {
        const float value = latent[dim];
        const float delta = value - mean[dim];
        mean[dim] += alpha * delta;
        variance[dim] = (1.f - alpha) * (variance[dim] + alpha * delta * delta);

        const float deviation = std::sqrt(variance[dim]) + 1.0e-4f;
        normalised[dim] = std::tanh(0.6f * (value - mean[dim]) / deviation);
    }
}

float LatentNormaliser::getNormalised(int dimension) const {
    if (dimension < 0 || dimension >= (int) normalised.size()) return 0.f;
    return normalised[(size_t) dimension];
}

ProcessorAmbisonics::ProcessorAmbisonics(juce::AudioProcessorValueTreeState &apvts) {
    auto get = [&apvts] (const juce::ParameterID& id) { return apvts.getRawParameterValue(id.getParamID()); };

    sources[0].parameters = {get(PluginParameters::AMBI_AZIMUTH_NETWORK1_ID),
                             get(PluginParameters::AMBI_ELEVATION_NETWORK1_ID),
                             get(PluginParameters::AMBI_SPREAD_NETWORK1_ID),
                             get(PluginParameters::AMBI_AZIMUTH_DEPTH_NETWORK1_ID),
                             get(PluginParameters::AMBI_ELEVATION_DEPTH_NETWORK1_ID),
                             get(PluginParameters::AMBI_SPREAD_DEPTH_NETWORK1_ID),
                             get(PluginParameters::AMBI_AZIMUTH_LATENT_NETWORK1_ID),
                             get(PluginParameters::AMBI_ELEVATION_LATENT_NETWORK1_ID),
                             get(PluginParameters::AMBI_SPREAD_LATENT_NETWORK1_ID)};

    sources[1].parameters = {get(PluginParameters::AMBI_AZIMUTH_NETWORK2_ID),
                             get(PluginParameters::AMBI_ELEVATION_NETWORK2_ID),
                             get(PluginParameters::AMBI_SPREAD_NETWORK2_ID),
                             get(PluginParameters::AMBI_AZIMUTH_DEPTH_NETWORK2_ID),
                             get(PluginParameters::AMBI_ELEVATION_DEPTH_NETWORK2_ID),
                             get(PluginParameters::AMBI_SPREAD_DEPTH_NETWORK2_ID),
                             get(PluginParameters::AMBI_AZIMUTH_LATENT_NETWORK2_ID),
                             get(PluginParameters::AMBI_ELEVATION_LATENT_NETWORK2_ID),
                             get(PluginParameters::AMBI_SPREAD_LATENT_NETWORK2_ID)};

    smoothingTime = get(PluginParameters::AMBI_SMOOTHING_ID);
    dryWidth = get(PluginParameters::AMBI_DRY_WIDTH_ID);
}

void ProcessorAmbisonics::prepare(double newSampleRate, int order) {
    sampleRate = newSampleRate;
    for (auto& source : sources) {
        source.encoder.prepare(order);
        source.normaliser.reset();
        source.initialised = false;
    }
    dryEncoderLeft.prepare(order);
    dryEncoderRight.prepare(order);
}

float ProcessorAmbisonics::getSmoothingCoefficient(int numSamples) const {
    const float tau = smoothingTime->load() * 0.001f;
    const float blockDuration = (float) numSamples / (float) sampleRate;
    return std::exp(-blockDuration / juce::jmax(tau, 1.0e-3f));
}

void ProcessorAmbisonics::encodeNetwork(int network, const juce::AudioBuffer<float> &mono, const std::vector<float> &latent,
                                        int numLatentDimensions, float gain, juce::AudioBuffer<float> &output) {
    auto& source = sources[(size_t) juce::jlimit(0, 1, network - 1)];
    auto& p = source.parameters;
    const int numSamples = mono.getNumSamples();

    source.normaliser.process(latent, numLatentDimensions, (float) numSamples / (float) sampleRate);

    const float azimuthMod = source.normaliser.getNormalised((int) p.azimuthLatent->load());
    const float elevationMod = source.normaliser.getNormalised((int) p.elevationLatent->load());
    const float spreadMod = source.normaliser.getNormalised((int) p.spreadLatent->load());

    const float azimuth = juce::degreesToRadians(p.azimuth->load() + p.azimuthDepth->load() * 180.f * azimuthMod);
    const float elevation = juce::degreesToRadians(juce::jlimit(-90.f, 90.f,
                                                   p.elevation->load() + p.elevationDepth->load() * 90.f * elevationMod));
    const float targetSpread = juce::jlimit(0.f, 1.f, p.spread->load() + p.spreadDepth->load() * spreadMod);

    const juce::Vector3D<float> targetDirection {std::cos(elevation) * std::cos(azimuth),
                                                 std::cos(elevation) * std::sin(azimuth),
                                                 std::sin(elevation)};

    // smoothing the direction vector instead of the angles avoids jumps at +/-180 degrees
    if (!source.initialised) {
        source.direction = targetDirection;
        source.spread = targetSpread;
        source.initialised = true;
    } else {
        const float a = getSmoothingCoefficient(numSamples);
        source.direction = source.direction * a + targetDirection * (1.f - a);
        source.spread = source.spread * a + targetSpread * (1.f - a);
    }

    auto length = source.direction.length();
    auto direction = length > 1.0e-6f ? source.direction / length : targetDirection;

    source.currentAzimuth = juce::radiansToDegrees(std::atan2(direction.y, direction.x));
    source.currentElevation = juce::radiansToDegrees(std::asin(juce::jlimit(-1.f, 1.f, direction.z)));

    AmbisonicEncoder::Coefficients coefficients;
    AmbisonicEncoder::computeCoefficients(direction.x, direction.y, direction.z, source.spread, coefficients);
    source.encoder.setTarget(coefficients, gain);
    source.encoder.encodeAdding(mono.getReadPointer(0), output, numSamples);
}

void ProcessorAmbisonics::encodeDry(const juce::AudioBuffer<float> &input, int numInputChannels, juce::AudioBuffer<float> &output) {
    const int numSamples = input.getNumSamples();
    AmbisonicEncoder::Coefficients coefficients;

    if (numInputChannels >= 2) {
        const float width = dryWidth->load();
        AmbisonicEncoder::computeCoefficients(width, 0.f, 0.f, coefficients);
        dryEncoderLeft.setTarget(coefficients, 1.f);
        dryEncoderLeft.encodeAdding(input.getReadPointer(0), output, numSamples);

        AmbisonicEncoder::computeCoefficients(-width, 0.f, 0.f, coefficients);
        dryEncoderRight.setTarget(coefficients, 1.f);
        dryEncoderRight.encodeAdding(input.getReadPointer(1), output, numSamples);
    } else if (numInputChannels == 1) {
        AmbisonicEncoder::computeCoefficients(0.f, 0.f, 0.f, coefficients);
        dryEncoderLeft.setTarget(coefficients, 1.f);
        dryEncoderLeft.encodeAdding(input.getReadPointer(0), output, numSamples);
    }
}

float ProcessorAmbisonics::getCurrentAzimuth(int network) const {
    return sources[(size_t) juce::jlimit(0, 1, network - 1)].currentAzimuth;
}

float ProcessorAmbisonics::getCurrentElevation(int network) const {
    return sources[(size_t) juce::jlimit(0, 1, network - 1)].currentElevation;
}

//
// Created by valentin.ackva on 16.03.2023.
//

#include "OnnxProcessor.h"

OnnxProcessor::OnnxProcessor(juce::AudioProcessorValueTreeState &apvts, int no, RaveModel raveModel) : inferenceThread(raveModel), number(no), parameters(apvts)
{
    inferenceThread.onNewProcessedBuffer = [this] (juce::AudioBuffer<float> buffer) {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            receiveRingBuffer.pushSample(buffer.getSample(0, sample), 0);
        }
        return 0;
    };

    inferenceThread.onNewLatentFrames = [this] (const float* latent, int numDimensions, int numFrames) {
        if (numFrames <= 0) return;
        auto dims = juce::jmin(numDimensions, maxLatentDimensions);
        for (int frame = 0; frame < numFrames; ++frame) {
            for (int dim = 0; dim < dims; ++dim) {
                latentRingBuffer.pushSample(latent[dim * numFrames + frame], dim);
            }
        }
        latentHopSize = inferenceThread.getModelInputSize() / numFrames;
        numLatentDimensions = dims;
    };

    inferenceThread.onModelLoaded = [this] (juce::String modelName) {
        onOnnxModelLoad(false, modelName);
        receiveRingBuffer.reset();
        latentRingBuffer.reset();
        numLatentDimensions = 0;
        samplesUntilNextLatentFrame = 0;
        std::fill(currentLatent.begin(), currentLatent.end(), 0.f);
        inferenceCounter = 0;
    };
}

void OnnxProcessor::parameterChanged(const juce::String &parameterID, float newValue) {
    if (parameterID == PluginParameters::SELECT_NETWORK1_ID.getParamID() && number == 1) {
        auto newValueBool = (bool) newValue;
        if (!newValueBool) {
            onOnnxModelLoad(true, "");
            inferenceThread.setInternalModel();
        }
    } else if (parameterID == PluginParameters::SELECT_NETWORK2_ID.getParamID() && number == 2) {
        auto newValueBool = (bool) newValue;
        if (!newValueBool) {
            onOnnxModelLoad(true, "");
            inferenceThread.setInternalModel();
        }
    } else if (parameterID == PluginParameters::ON_OFF_NETWORK1_ID.getParamID() && number == 1) {
//        setMuted(!(bool) newValue);
    } else if (parameterID == PluginParameters::ON_OFF_NETWORK2_ID.getParamID() && number == 2) {
//        setMuted(!(bool)newValue);
    }
}

void OnnxProcessor::prepare(const juce::dsp::ProcessSpec &spec) {
    receiveRingBuffer.initialise(1, (int) spec.sampleRate);
    // one latent frame per model hop, a second of audio needs far less than this
    latentRingBuffer.initialise(maxLatentDimensions, 512);
    currentLatent.assign(maxLatentDimensions, 0.f);
    numLatentDimensions = 0;
    samplesUntilNextLatentFrame = 0;
    monoBuffer.setSize(1, (int) spec.maximumBlockSize);
    inferenceThread.prepare(spec);
    inferenceCounter = 0;
    calculateLatency((int)spec.maximumBlockSize);

    if (spec.sampleRate != 48000.0) {
        warningWindow.showWarningWindow(SampleRateWarning);
    }
}

void OnnxProcessor::processBlock(juce::AudioBuffer<float> &buffer) {
    const int numSamples = buffer.getNumSamples();
    inferenceThread.sendAudio(buffer);
    processOutput(buffer, numSamples);
}

void OnnxProcessor::processOutput(juce::AudioBuffer<float> &buffer, const int numSamples) {
    auto availableSamples = receiveRingBuffer.getAvailableSamples(0);
    if (!inferenceThread.init){
        if (availableSamples >= numSamples) {
            if (inferenceCounter > 0) {
                if (availableSamples >= 2 * numSamples) {
                    advanceLatent(numSamples);
                    for (int i = 0; i < numSamples; ++i) {
                        receiveRingBuffer.popSample(0);
                    }
                    inferenceCounter--;
                }
            }
            advanceLatent(numSamples);
            for (int sample = 0; sample < numSamples; ++sample) {
                buffer.setSample(0, sample, receiveRingBuffer.popSample(0));
            }
        } else {
            inferenceCounter++;
            std::cout << "missing samples" << std::endl;
            for (int sample = 0; sample < numSamples; ++sample) {
                buffer.setSample(0, sample, 0.0f);
            }
        }
    }
}

void OnnxProcessor::advanceLatent(int numSamples) {
    // latent frames are pushed before their audio, so every frame that starts within the
    // popped samples is already available. The last frame that starts in this block wins.
    const int hop = latentHopSize;
    if (hop <= 0) return;
    const int dims = numLatentDimensions;

    while (samplesUntilNextLatentFrame < numSamples) {
        if (latentRingBuffer.getAvailableSamples(0) <= 0) {
            samplesUntilNextLatentFrame = 0;
            return;
        }
        for (int dim = 0; dim < dims; ++dim) {
            currentLatent[(size_t) dim] = latentRingBuffer.popSample(dim);
        }
        samplesUntilNextLatentFrame += hop;
    }
    samplesUntilNextLatentFrame -= numSamples;
}

const std::vector<float>& OnnxProcessor::getCurrentLatent() const {
    return currentLatent;
}

int OnnxProcessor::getNumLatentDimensions() const {
    return numLatentDimensions;
}

void OnnxProcessor::loadExternalModel(juce::File file) {
    onOnnxModelLoad(true, file.getFileNameWithoutExtension());
    inferenceThread.setExternalModel(file);
}

void OnnxProcessor::calculateLatency(int maxSamplesPerBuffer) {
    float latency = (float) (inferenceThread.getLatency()) / (float) maxSamplesPerBuffer;
    if (latency == static_cast<float>(static_cast<int>(latency))) latencyInSamples = static_cast<int>(latency) * maxSamplesPerBuffer - maxSamplesPerBuffer;
    else latencyInSamples = static_cast<int>((latency + 1.f)) * maxSamplesPerBuffer - maxSamplesPerBuffer;
}

int OnnxProcessor::getLatency() const {
    return latencyInSamples;
}

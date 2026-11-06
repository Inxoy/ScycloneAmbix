//
// Created by valentin.ackva on 10.03.2023.
//

#ifndef VAESYNTH_ONNXPROCESSOR_H
#define VAESYNTH_ONNXPROCESSOR_H

#include "JuceHeader.h"
#include "RingBuffer.h"
#include "InferenceThread.h"
#include "../../PluginParameters.h"
#include "WarningWindow.h"
#include "ModelLibrary.h"
#include "StreamingResampler.h"

class OnnxProcessor {
public:
    OnnxProcessor(juce::AudioProcessorValueTreeState &apvts, int no, RaveModel raveModel);

    void parameterChanged(const juce::String &parameterID, float newValue);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void processBlock(juce::AudioBuffer<float>& buffer);
    int getLatency() const;
    void loadExternalModel(juce::File path);
    // index into ModelLibrary::getEntries()
    bool loadLibraryModel(int index);

    // latent frame that belongs to the audio returned by the last processBlock call
    const std::vector<float>& getCurrentLatent() const;
    int getNumLatentDimensions() const;

    std::function<void(bool initLoading, juce::String modelName)> onOnnxModelLoad;

    static constexpr int maxLatentDimensions = 128;

private:
    void processOutput(juce::AudioBuffer<float>& buffer, int numSamples);
    // streaming models (converted RAVE exports) run at their own rate and are delayed by
    // exactly the reported latency
    void configureStreaming();
    void processStreaming(juce::AudioBuffer<float>& buffer, int numSamples);
    void pushProcessed(const juce::AudioBuffer<float>& modelRateBuffer);
    void advanceLatent(int numSamples);
    void calculateLatency(int maxSamplesPerBuffer);


private:
    juce::AudioProcessorValueTreeState& parameters;

    InferenceThread inferenceThread;
    int latencyInSamples = 0;
    RingBuffer receiveRingBuffer;
    RingBuffer latentRingBuffer;
    std::vector<float> currentLatent;
    std::atomic<int> numLatentDimensions {0};
    std::atomic<int> latentHopSize {0};
    int samplesUntilNextLatentFrame = 0;
    juce::AudioBuffer<float> monoBuffer;
    int inferenceCounter = 0;
    std::unique_ptr<juce::FileChooser> fc;
    WarningWindow warningWindow;
    int number;

    std::atomic<bool> streaming {false};
    double hostSampleRate = 48000.0;
    int maxBlockSize = 512;
    StreamingResampler inputResampler, outputResampler;
    juce::AudioBuffer<float> modelRateBlock;     // host block converted to the model's rate
    std::vector<float> hostRateChunk;            // a processed chunk converted back
    std::atomic<int> modelLatentHop {0};
};

#endif //VAESYNTH_ONNXPROCESSOR_H

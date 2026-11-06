//
// Created by valentin.ackva on 10.03.2023.
//

#ifndef VAESYNTH_INFERENCETHREAD_H
#define VAESYNTH_INFERENCETHREAD_H


#include "JuceHeader.h"
#include "onnxruntime_cxx_api.h"
#include "RingBuffer.h"
#include "chrono"

enum RaveModel {
    FunkDrum,
    Djembe
};

class InferenceThread : public juce::Thread {
public:
    InferenceThread(RaveModel raveModel);
    ~InferenceThread() override;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void sendAudio(juce::AudioBuffer<float>& buffer);
    void setExternalModel(juce::File modelPath, const juce::String& name = {});
    void setEmbeddedModel(RaveModel model, const juce::String& name);
    int getLatency();
    int getModelInputSize() const { return modelInputSize; }

    // Streaming models (tools/convert_rave_ts.py) keep convolution caches between calls and
    // run at their own sample rate; Scyclone's own models are neither.
    bool isStreaming() const { return streaming; }
    double getModelSampleRate() const { return modelSampleRate; }

    std::function<int(juce::AudioBuffer<float> buffer)> onNewProcessedBuffer;
    // latent data is laid out as [dimension][frame], called before onNewProcessedBuffer
    std::function<void(const float* latent, int numDimensions, int numFrames)> onNewLatentFrames;
    std::function<void(juce::String modelName)> onModelLoaded;
    
    bool init = true;
    int init_samples = 0;
    void setInternalModel();

private:
    void run() override;

    void modelInputSizeChanged(int newModelInputSize);
    void loadExternalModel(juce::File modelPath, const juce::String& name);
    void loadInternalModel(RaveModel modelToLoad, const juce::String& name = {});
    std::vector<int> getInputShape(Ort::Session *sess);
    void checkLatentOutput();
    void inspectModel();
    void runStreaming();
    void resetStates();

private:
    bool startUp = true;
    juce::dsp::ProcessSpec last_spec;

    RaveModel currentLevel;

    Ort::Env env;
    Ort::RunOptions runOptions;
    Ort::Session session;

    std::vector<float> onnxInputData;
    std::vector<float> onnxOutputData;

    juce::AudioBuffer<float> processedBuffer;
    int maxModelCalcSize = 4096;
    int modelInputSize = 16384;
    RingBuffer receiveRingBuffer;

    bool loadingModel = false;
    // models exported with tools/add_latent_output.py provide the latent as second output
    bool hasLatentOutput = false;

    // streaming models: the cache of every convolution goes in as "state_<n>" and comes
    // back as "new_state_<n>"
    struct State {
        std::string inputName, outputName;
        std::vector<int64_t> shape;
        std::vector<float> data;
        int outputIndex = -1;
    };
    bool streaming = false;
    double modelSampleRate = 48000.0;
    std::vector<State> states;
    std::vector<std::string> inputNames, outputNames;
    int audioOutputIndex = 0, latentOutputIndex = -1;
};
#endif //VAESYNTH_INFERENCETHREAD_H

//
// Created by valentin.ackva on 16.03.2023.
//

#include "InferenceThread.h"

InferenceThread::InferenceThread(RaveModel raveModel) : juce::Thread("OnnxInference"), session(nullptr), currentLevel(raveModel){
    modelInputSizeChanged(modelInputSize);
    setInternalModel();
}

InferenceThread::~InferenceThread() {
    session.release();
    stopThread(100);
}

void InferenceThread::prepare(const juce::dsp::ProcessSpec &spec) {
    // allocate enough memory
    receiveRingBuffer.initialise(1, (int) spec.sampleRate);
    resetStates();

    last_spec = spec;
    init = true;
    init_samples = 0;
}

void InferenceThread::sendAudio(juce::AudioBuffer<float> &buffer) {
    auto readPointer = buffer.getReadPointer(0);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        receiveRingBuffer.pushSample(readPointer[sample], 0);
        if (init) init_samples++;
    }

    if (!isThreadRunning() && receiveRingBuffer.getAvailableSamples(0) >= modelInputSize && !loadingModel) {

        for (float & sample : onnxInputData) {
            sample = receiveRingBuffer.popSample(0);
        }

        startThread(juce::Thread::Priority::highest);
    }
    if (init && init_samples >= modelInputSize + maxModelCalcSize) init = false;
}

void InferenceThread::run() {
    if (streaming) {
        runStreaming();
        return;
    }

    auto start = std::chrono::high_resolution_clock::now();

    //process
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

    // define shape
    std::array<int64_t, 3> inputShape = {1, 1, modelInputSize};
    std::array<int64_t, 3> outputShape = {1, 1, modelInputSize};

    auto inputTensor = std::make_shared<Ort::Value>(Ort::Value::CreateTensor<float>(memory_info,
                                                                                    onnxInputData.data(),
                                                                                    modelInputSize,
                                                                                    inputShape.data(),
                                                                                    inputShape.size()));

    auto outputTensor = std::make_shared<Ort::Value>(Ort::Value::CreateTensor<float>(memory_info,
                                                                                     onnxOutputData.data(),
                                                                                     modelInputSize,
                                                                                     outputShape.data(),
                                                                                     outputShape.size()));

    Ort::AllocatorWithDefaultOptions ort_alloc;

    Ort::AllocatedStringPtr inputName = session.GetInputNameAllocated(0, ort_alloc);
    Ort::AllocatedStringPtr outputName = session.GetOutputNameAllocated(0, ort_alloc);
    const std::array<const char *, 1> inputNames = {(char*) inputName.get()};

    // run inference
    if (hasLatentOutput) {
        Ort::AllocatedStringPtr latentName = session.GetOutputNameAllocated(1, ort_alloc);
        const std::array<const char *, 2> outputNames = {(char*) outputName.get(), (char*) latentName.get()};
        // the latent tensor is allocated by onnxruntime, its shape is [1, dimensions, frames]
        std::array<Ort::Value, 2> outputValues = {std::move(*outputTensor), Ort::Value(nullptr)};
        try {
            session.Run(runOptions, inputNames.data(), inputTensor.get(), 1, outputNames.data(), outputValues.data(), 2);
            auto latentShape = outputValues[1].GetTensorTypeAndShapeInfo().GetShape();
            if (latentShape.size() == 3 && onNewLatentFrames)
                onNewLatentFrames(outputValues[1].GetTensorData<float>(), (int) latentShape[1], (int) latentShape[2]);
        } catch (Ort::Exception &e) {
            std::cout << e.what() << std::endl;
        }
    } else {
        const std::array<const char *, 1> outputNames = {(char*) outputName.get()};
        try {
            session.Run(runOptions, inputNames.data(), inputTensor.get(), 1, outputNames.data(), outputTensor.get(), 1);
        } catch (Ort::Exception &e) {
            std::cout << e.what() << std::endl;
        }
    }

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    // std::cout << duration.count() << "ms" << std::endl;

    for (int i = 0; i < processedBuffer.getNumSamples(); ++i) {
        processedBuffer.setSample(0, i, onnxOutputData[i]);
    }

    onNewProcessedBuffer(processedBuffer);
//    ort_alloc.Free(inputName.get());
}

void InferenceThread::setExternalModel(juce::File modelPath, const juce::String& name) {
    loadExternalModel(modelPath, name);
}

void InferenceThread::modelInputSizeChanged(int newModelInputSize) {
    modelInputSize = newModelInputSize;
    processedBuffer.setSize(1, newModelInputSize);

    onnxInputData.resize(newModelInputSize, 0.0f);
    onnxOutputData.resize(newModelInputSize, 0.0f);
}

void InferenceThread::loadExternalModel(juce::File modelPath, const juce::String& name) {
    loadingModel = true;

    if (isThreadRunning()) {
        stopThread(10);

        while (!isThreadRunning()) {
            juce::Time::waitForMillisecondCounter(juce::Time::getMillisecondCounter() + 1);
        }
    }

    Ort::SessionOptions sessionOptions;



#if JUCE_WINDOWS
    auto modelPathToLoad = modelPath.getFullPathName().toStdString();
    std::wstring modelWideStr = std::wstring(modelPathToLoad.begin(), modelPathToLoad.end());
    const wchar_t* modelWideCStr = modelWideStr.c_str();

    session = Ort::Session(env,
                       modelWideCStr,
                       sessionOptions);
#else
    auto modelPathToLoad = modelPath.getFullPathName().toStdString();
    const char* modelCStr = modelPathToLoad.c_str();

    session = Ort::Session(env,
                           modelCStr,
                           sessionOptions);
#endif

    prepare(last_spec);
    checkLatentOutput();

    auto shape = getInputShape(&session);

    for (int i = 0; i < shape.size(); ++i) {
//        std::cout << "shape[" <<  i << "]: " << shape[i] << std::endl;
    }
    onModelLoaded(name.isNotEmpty() ? name : modelPath.getFileNameWithoutExtension());
    loadingModel = false;
}

void InferenceThread::setEmbeddedModel(RaveModel model, const juce::String& name) {
    currentLevel = model;
    loadInternalModel(model, name);
}

void InferenceThread::loadInternalModel(RaveModel modelToLoad, const juce::String& name) {
    loadingModel = true;

    if (isThreadRunning()) {
        stopThread(10);

        while (!isThreadRunning()) {
            juce::Time::waitForMillisecondCounter(juce::Time::getMillisecondCounter() + 1);
        }
    }

    Ort::SessionOptions sessionOptions;

    switch (modelToLoad) {
        default:
            //not implemented
        case FunkDrum:
            session = Ort::Session(env,
                                   BinaryData::funk_drums_ort,
                                   BinaryData::funk_drums_ortSize,
                                   sessionOptions);
            break;
        case Djembe:
            session = Ort::Session(env,
                                   BinaryData::djembe_ort,
                                   BinaryData::djembe_ortSize,
                                   sessionOptions);
            break;
    }
    if (! startUp){
        prepare(last_spec);
    }
    checkLatentOutput();

    auto shape = getInputShape(&session);

    for (int i = 0; i < shape.size(); ++i) {
//        std::cout << "shape[" <<  i << "]: " << shape[i] << std::endl;
    }
    if (! startUp){
        onModelLoaded(name);
    }
    loadingModel = false;
    startUp = false;
}

std::vector<int> InferenceThread::getInputShape(Ort::Session *sess) {
    std::vector<int> returnVec;
    std::vector<int64_t> inputShape = sess->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    for (long long dim : inputShape) {
        returnVec.push_back(static_cast<int>(dim));
    }
    return returnVec;
}

void InferenceThread::checkLatentOutput() {
    inspectModel();
}

void InferenceThread::inspectModel() {
    Ort::AllocatorWithDefaultOptions allocator;

    inputNames.clear();
    outputNames.clear();
    for (size_t i = 0; i < session.GetInputCount(); ++i)
        inputNames.emplace_back(session.GetInputNameAllocated(i, allocator).get());
    for (size_t i = 0; i < session.GetOutputCount(); ++i)
        outputNames.emplace_back(session.GetOutputNameAllocated(i, allocator).get());

    auto metadataValue = [&] (const char* key) -> std::string {
        auto metadata = session.GetModelMetadata();
        auto value = metadata.LookupCustomMetadataMapAllocated(key, allocator);
        return value ? std::string(value.get()) : std::string();
    };

    streaming = metadataValue("streaming") == "1";
    const auto rate = metadataValue("sample_rate");
    modelSampleRate = (streaming && !rate.empty()) ? std::stod(rate) : 48000.0;

    states.clear();
    audioOutputIndex = 0;
    latentOutputIndex = outputNames.size() >= 2 ? 1 : -1;

    if (streaming) {
        auto indexOf = [this] (const std::string& name) {
            for (size_t i = 0; i < outputNames.size(); ++i)
                if (outputNames[i] == name) return (int) i;
            return -1;
        };
        audioOutputIndex = juce::jmax(0, indexOf("audio"));
        latentOutputIndex = indexOf("latent");

        for (size_t i = 1; i < inputNames.size(); ++i) {
            State state;
            state.inputName = inputNames[i];
            state.outputName = "new_" + inputNames[i];
            state.outputIndex = indexOf(state.outputName);
            state.shape = session.GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
            size_t size = 1;
            for (auto& d : state.shape) {
                if (d < 0) d = 1;
                size *= (size_t) d;
            }
            state.data.assign(size, 0.f);
            states.push_back(std::move(state));
        }

        // chunk length at the model's rate, fixed by the export
        const auto inputShape = session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        if (inputShape.size() == 3 && inputShape[2] > 0)
            modelInputSizeChanged((int) inputShape[2]);
    } else if (modelInputSize != 16384) {
        modelInputSizeChanged(16384);
    }

    hasLatentOutput = latentOutputIndex >= 0;
}

void InferenceThread::resetStates() {
    for (auto& state : states)
        std::fill(state.data.begin(), state.data.end(), 0.f);
}

void InferenceThread::runStreaming() {
    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

    std::vector<Ort::Value> inputs;
    std::vector<const char*> inputNamePointers;
    inputs.reserve(states.size() + 1);

    const std::array<int64_t, 3> inputShape = {1, 1, modelInputSize};
    inputs.push_back(Ort::Value::CreateTensor<float>(memoryInfo, onnxInputData.data(), (size_t) modelInputSize,
                                                     inputShape.data(), inputShape.size()));
    inputNamePointers.push_back(inputNames[0].c_str());
    for (auto& state : states) {
        inputs.push_back(Ort::Value::CreateTensor<float>(memoryInfo, state.data.data(), state.data.size(),
                                                         state.shape.data(), state.shape.size()));
        inputNamePointers.push_back(state.inputName.c_str());
    }

    std::vector<const char*> outputNamePointers;
    for (auto& name : outputNames) outputNamePointers.push_back(name.c_str());

    try {
        auto outputs = session.Run(runOptions, inputNamePointers.data(), inputs.data(), inputs.size(),
                                   outputNamePointers.data(), outputNamePointers.size());

        const float* audio = outputs[(size_t) audioOutputIndex].GetTensorData<float>();
        const auto audioCount = outputs[(size_t) audioOutputIndex].GetTensorTypeAndShapeInfo().GetElementCount();
        for (int i = 0; i < modelInputSize; ++i)
            onnxOutputData[(size_t) i] = (size_t) i < audioCount ? audio[i] : 0.f;

        if (latentOutputIndex >= 0 && onNewLatentFrames) {
            auto latentShape = outputs[(size_t) latentOutputIndex].GetTensorTypeAndShapeInfo().GetShape();
            if (latentShape.size() == 3)
                onNewLatentFrames(outputs[(size_t) latentOutputIndex].GetTensorData<float>(),
                                  (int) latentShape[1], (int) latentShape[2]);
        }

        // the caches for the next chunk
        for (auto& state : states) {
            if (state.outputIndex < 0) continue;
            const auto& value = outputs[(size_t) state.outputIndex];
            const auto count = juce::jmin(value.GetTensorTypeAndShapeInfo().GetElementCount(), state.data.size());
            std::copy_n(value.GetTensorData<float>(), count, state.data.begin());
        }
    } catch (Ort::Exception& e) {
        std::cout << e.what() << std::endl;
        std::fill(onnxOutputData.begin(), onnxOutputData.end(), 0.f);
    }

    for (int i = 0; i < processedBuffer.getNumSamples(); ++i)
        processedBuffer.setSample(0, i, onnxOutputData[(size_t) i]);

    onNewProcessedBuffer(processedBuffer);
}

int InferenceThread::getLatency(){
    // the same for every model (Scyclone's are 16384 samples long), so the plugin's latency
    // doesn't change with the model; streaming models use shorter chunks inside it
    return 16384 + maxModelCalcSize;
}

void InferenceThread::setInternalModel() {
    switch(currentLevel) {
        case FunkDrum:
            loadInternalModel(FunkDrum);
            break;
        case Djembe:
            loadInternalModel(Djembe);
            break;
        default:
            loadInternalModel(FunkDrum);
    }
}

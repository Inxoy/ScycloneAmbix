//
// Offline checks for the ambisonic encoding. Build with -DSCYCLONE_BUILD_TESTS=ON and run
// ScycloneAmbixTest, it returns a non-zero exit code if a check fails.
//

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "dsp/ambisonics/AmbisonicEncoder.h"

namespace {
    int failures = 0;

    void check(bool condition, const juce::String& message) {
        std::cout << (condition ? "  ok    " : "  FAIL  ") << message << std::endl;
        if (!condition) ++failures;
    }

    void testEncoderNormalisation() {
        std::cout << "encoder normalisation (SN3D)" << std::endl;
        juce::Random random(42);
        float worstError = 0.f;
        for (int i = 0; i < 1000; ++i) {
            AmbisonicEncoder::Coefficients c;
            AmbisonicEncoder::computeCoefficients(random.nextFloat() * 360.f - 180.f,
                                                  random.nextFloat() * 180.f - 90.f, 0.f, c);
            // with SN3D, the squared harmonics of every order sum up to one
            for (int n = 0; n <= AmbisonicEncoder::maxOrder; ++n) {
                float sum = 0.f;
                for (int acn = n * n; acn < (n + 1) * (n + 1); ++acn) sum += c[(size_t) acn] * c[(size_t) acn];
                worstError = juce::jmax(worstError, std::abs(sum - 1.f));
            }
        }
        check(worstError < 1.0e-4f, "sum of squares per order is 1 (max error " + juce::String(worstError) + ")");

        AmbisonicEncoder::Coefficients left, up, spread;
        AmbisonicEncoder::computeCoefficients(90.f, 0.f, 0.f, left);
        check(std::abs(left[1] - 1.f) < 1.0e-5f && std::abs(left[3]) < 1.0e-5f, "azimuth +90 is left (Y = 1, X = 0)");
        AmbisonicEncoder::computeCoefficients(0.f, 90.f, 0.f, up);
        check(std::abs(up[2] - 1.f) < 1.0e-5f, "elevation +90 is up (Z = 1)");
        AmbisonicEncoder::computeCoefficients(45.f, 20.f, 1.f, spread);
        float higher = 0.f;
        for (int acn = 1; acn < AmbisonicEncoder::maxChannels; ++acn) higher += std::abs(spread[(size_t) acn]);
        check(spread[0] == 1.f && higher < 1.0e-6f, "full spread leaves only W");
    }

    // drum-like test signal: decaying noise bursts with changing pitch and brightness
    void fillTestSignal(juce::AudioBuffer<float>& buffer, int64_t startSample, double sampleRate, juce::Random& random) {
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            const auto t = (double) (startSample + i) / sampleRate;
            const auto beat = std::fmod(t, 0.25);
            const auto bar = (int) (t / 0.25) % 8;
            const auto envelope = std::exp(-beat * (bar % 2 == 0 ? 18.0 : 40.0));
            const auto tone = std::sin(2.0 * juce::MathConstants<double>::pi * (60.0 + 40.0 * bar) * beat);
            const auto noise = (random.nextFloat() * 2.f - 1.f) * (bar % 4 == 1 ? 0.8f : 0.2f);
            const auto sample = (float) (0.5 * envelope * (tone + noise));
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.setSample(ch, i, sample);
        }
    }

    struct RunResult {
        float minAzimuth = 1000.f, maxAzimuth = -1000.f;
        float minElevation = 1000.f, maxElevation = -1000.f;
        float intensityAzimuthRange = 0.f;
        float meanIntensityAzimuth = 0.f;
        std::vector<double> channelEnergy;
        bool finite = true;
    };

    // layoutAfterPrepare imitates hosts that change the bus layout without preparing again
    RunResult run(const juce::AudioChannelSet& outputLayout, double seconds, bool setAmbiParameters,
                  const juce::AudioChannelSet& layoutAfterPrepare = {}) {
        std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(outputLayout);
        check(processor->setBusesLayout(layout), "bus layout " + outputLayout.getDescription() + " accepted");

        if (setAmbiParameters) {
            // only the network output so the spatial movement is not masked by the dry signal
            for (auto* parameter : processor->getParameters()) {
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(parameter)) {
                    if (p->paramID == PluginParameters::DRY_WET_ID.getParamID()) p->setValueNotifyingHost(1.f);
                    if (p->paramID == PluginParameters::AMBI_AZIMUTH_DEPTH_NETWORK1_ID.getParamID()) p->setValueNotifyingHost(1.f);
                    if (p->paramID == PluginParameters::AMBI_ELEVATION_DEPTH_NETWORK1_ID.getParamID()) p->setValueNotifyingHost(0.5f);
                    if (p->paramID == PluginParameters::AMBI_SMOOTHING_ID.getParamID()) p->setValueNotifyingHost(0.1f);
                }
            }
        }

        const double sampleRate = 48000.0;
        const int blockSize = 512;
        processor->prepareToPlay(sampleRate, blockSize);

        if (layoutAfterPrepare.size() > 0) {
            layout.outputBuses.getReference(0) = layoutAfterPrepare;
            check(processor->setBusesLayout(layout), "switched to " + layoutAfterPrepare.getDescription() + " after prepare");
        }

        const int numChannels = juce::jmax(processor->getTotalNumInputChannels(), processor->getTotalNumOutputChannels());
        juce::AudioBuffer<float> buffer(numChannels, blockSize);
        juce::MidiBuffer midi;
        juce::Random random(7);

        auto* scyclone = dynamic_cast<AudioPluginAudioProcessor*>(processor.get());
        RunResult result;
        result.channelEnergy.assign((size_t) processor->getTotalNumOutputChannels(), 0.0);

        const auto totalBlocks = (int64_t) (seconds * sampleRate / blockSize);
        double intensityX = 0.0, intensityY = 0.0;
        double totalIntensityX = 0.0, totalIntensityY = 0.0;
        float minIntensityAzimuth = 1000.f, maxIntensityAzimuth = -1000.f;
        const auto blockDurationMs = 1000.0 * blockSize / sampleRate;
        auto start = juce::Time::getMillisecondCounterHiRes();

        for (int64_t block = 0; block < totalBlocks; ++block) {
            buffer.clear();
            fillTestSignal(buffer, block * blockSize, sampleRate, random);
            for (int ch = processor->getTotalNumInputChannels(); ch < numChannels; ++ch) buffer.clear(ch, 0, blockSize);

            processor->processBlock(buffer, midi);

            // inference runs on its own thread, so the test runs in real time
            const auto due = start + (double) (block + 1) * blockDurationMs;
            while (juce::Time::getMillisecondCounterHiRes() < due) juce::Thread::sleep(1);

            // skip the start up latency of the networks
            if ((double) block * blockSize / sampleRate < 1.0) continue;

            for (int ch = 0; ch < processor->getTotalNumOutputChannels(); ++ch) {
                const auto* read = buffer.getReadPointer(ch);
                for (int i = 0; i < blockSize; ++i) {
                    result.channelEnergy[(size_t) ch] += read[i] * read[i];
                    if (!std::isfinite(read[i])) result.finite = false;
                }
            }

            if (scyclone != nullptr && scyclone->getAmbisonicOrder() > 0) {
                const float azimuth = scyclone->getAmbisonicAzimuth(1);
                const float elevation = scyclone->getAmbisonicElevation(1);
                result.minAzimuth = juce::jmin(result.minAzimuth, azimuth);
                result.maxAzimuth = juce::jmax(result.maxAzimuth, azimuth);
                result.minElevation = juce::jmin(result.minElevation, elevation);
                result.maxElevation = juce::jmax(result.maxElevation, elevation);

                // direction of the encoded signal itself, from the first order intensity vector
                for (int i = 0; i < blockSize; ++i) {
                    intensityX += buffer.getSample(0, i) * buffer.getSample(3, i);
                    intensityY += buffer.getSample(0, i) * buffer.getSample(1, i);
                    totalIntensityX += buffer.getSample(0, i) * buffer.getSample(3, i);
                    totalIntensityY += buffer.getSample(0, i) * buffer.getSample(1, i);
                }
                if (block % 24 == 0) {
                    if (std::abs(intensityX) + std::abs(intensityY) > 1.0e-6) {
                        const auto a = (float) juce::radiansToDegrees(std::atan2(intensityY, intensityX));
                        minIntensityAzimuth = juce::jmin(minIntensityAzimuth, a);
                        maxIntensityAzimuth = juce::jmax(maxIntensityAzimuth, a);
                    }
                    intensityX = intensityY = 0.0;
                }
            }
        }
        result.intensityAzimuthRange = maxIntensityAzimuth - minIntensityAzimuth;
        result.meanIntensityAzimuth = (float) juce::radiansToDegrees(std::atan2(totalIntensityY, totalIntensityX));
        processor->releaseResources();
        return result;
    }
}

namespace {
    juce::Button* findButton(juce::Component& parent, const juce::String& text) {
        for (auto* child : parent.getChildren()) {
            if (auto* button = dynamic_cast<juce::Button*>(child); button != nullptr && button->getButtonText() == text)
                return button;
            if (auto* found = findButton(*child, text))
                return found;
        }
        return nullptr;
    }

    // feeds the test signal in real time while the message loop draws the editor
    class AudioFeeder : public juce::Thread {
    public:
        AudioFeeder(juce::AudioProcessor& p, double secondsToRun, std::function<void()> onDone)
                : juce::Thread("AudioFeeder"), processor(p), seconds(secondsToRun), done(std::move(onDone)) {}

        void run() override {
            const double sampleRate = 48000.0;
            const int blockSize = 512;
            const int numChannels = juce::jmax(processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels());
            juce::AudioBuffer<float> buffer(numChannels, blockSize);
            juce::MidiBuffer midi;
            juce::Random random(7);
            const auto start = juce::Time::getMillisecondCounterHiRes();
            const auto totalBlocks = (int64_t) (seconds * sampleRate / blockSize);

            for (int64_t block = 0; block < totalBlocks && !threadShouldExit(); ++block) {
                buffer.clear();
                fillTestSignal(buffer, block * blockSize, sampleRate, random);
                for (int ch = processor.getTotalNumInputChannels(); ch < numChannels; ++ch) buffer.clear(ch, 0, blockSize);
                processor.processBlock(buffer, midi);

                const auto due = start + (double) (block + 1) * 1000.0 * blockSize / sampleRate;
                while (juce::Time::getMillisecondCounterHiRes() < due) juce::Thread::sleep(1);
            }
            done();
        }

    private:
        juce::AudioProcessor& processor;
        double seconds;
        std::function<void()> done;
    };

    // renders the editor with the spatial view open to a PNG, for checking the layout
    int renderSnapshot(const juce::File& file) {
        std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::ambisonic(3));
        processor->setBusesLayout(layout);
        for (auto* parameter : processor->getParameters()) {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(parameter)) {
                if (p->paramID == PluginParameters::ON_OFF_NETWORK2_ID.getParamID()) p->setValueNotifyingHost(1.f);
                if (p->paramID == PluginParameters::FADE_ID.getParamID()) p->setValueNotifyingHost(0.5f);
                if (p->paramID == PluginParameters::AMBI_AZIMUTH_NETWORK2_ID.getParamID()) p->setValueNotifyingHost(p->convertTo0to1(-100.f));
                if (p->paramID == PluginParameters::AMBI_SPREAD_DEPTH_NETWORK2_ID.getParamID()) p->setValueNotifyingHost(0.4f);
            }
        }
        processor->prepareToPlay(48000.0, 512);

        std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
        editor->setVisible(true);
        auto* spatialButton = findButton(*editor, "Spatial");
        if (spatialButton == nullptr) {
            std::cout << "spatial button not found" << std::endl;
            return 1;
        }
        spatialButton->setToggleState(true, juce::dontSendNotification);
        spatialButton->onClick();

        bool written = false;
        AudioFeeder feeder(*processor, 8.0, [&] {
            juce::MessageManager::callAsync([&] {
                auto image = editor->createComponentSnapshot(editor->getLocalBounds());
                juce::FileOutputStream stream(file);
                if (stream.openedOk()) {
                    stream.setPosition(0);
                    stream.truncate();
                    written = juce::PNGImageFormat().writeImageToStream(image, stream);
                }
                juce::MessageManager::getInstance()->stopDispatchLoop();
            });
        });
        feeder.startThread();
        juce::MessageManager::getInstance()->runDispatchLoop();
        feeder.stopThread(2000);

        editor.reset();
        processor->releaseResources();
        std::cout << (written ? "wrote " + file.getFullPathName() : juce::String("could not write snapshot")) << std::endl;
        return written ? 0 : 1;
    }
}

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc == 3 && juce::String(argv[1]) == "--snapshot")
        return renderSnapshot(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]));

    if (argc == 2 && juce::String(argv[1]) == "--device-types") {
        // the audio device types the standalone offers, e.g. to check that ASIO is compiled in
        juce::AudioDeviceManager deviceManager;
        for (auto* type : deviceManager.getAvailableDeviceTypes()) {
            type->scanForDevices();
            std::cout << type->getTypeName() << ": " << type->getDeviceNames().joinIntoString(", ") << std::endl;
        }
        return 0;
    }

    testEncoderNormalisation();

    std::cout << "third order AmbiX output, latent modulation" << std::endl;
    auto ambi = run(juce::AudioChannelSet::ambisonic(3), 12.0, true);
    check(ambi.channelEnergy.size() == 16, "16 output channels");
    check(ambi.finite, "output is finite");
    check(ambi.channelEnergy[0] > 1.0, "W carries signal (energy " + juce::String(ambi.channelEnergy[0]) + ")");
    double higherOrderEnergy = 0.0;
    for (size_t ch = 4; ch < ambi.channelEnergy.size(); ++ch) higherOrderEnergy += ambi.channelEnergy[ch];
    check(higherOrderEnergy > 0.01 * ambi.channelEnergy[0], "second and third order carry signal");
    std::cout << "        source azimuth " << ambi.minAzimuth << " .. " << ambi.maxAzimuth
              << ", elevation " << ambi.minElevation << " .. " << ambi.maxElevation << std::endl;
    check(ambi.maxAzimuth - ambi.minAzimuth > 20.f, "latent moves the azimuth");
    check(ambi.maxElevation - ambi.minElevation > 5.f, "latent moves the elevation");
    check(ambi.maxElevation <= 90.f && ambi.minElevation >= -90.f, "elevation stays in range");
    check(ambi.intensityAzimuthRange > 20.f, "movement is audible in the encoded signal (intensity azimuth range "
                                              + juce::String(ambi.intensityAzimuthRange, 1) + ")");

    std::cout << "third order AmbiX output, default parameters" << std::endl;
    auto defaults = run(juce::AudioChannelSet::ambisonic(3), 10.0, false);
    std::cout << "        mean intensity azimuth " << defaults.meanIntensityAzimuth
              << ", Y/W energy " << defaults.channelEnergy[1] / defaults.channelEnergy[0] << std::endl;
    check(std::abs(defaults.meanIntensityAzimuth) < 45.f, "field is centred in front, not pulled to one side");

    std::cout << "first order AmbiX output" << std::endl;
    auto firstOrder = run(juce::AudioChannelSet::ambisonic(1), 4.0, false);
    check(firstOrder.channelEnergy.size() == 4 && firstOrder.finite && firstOrder.channelEnergy[0] > 1.0, "4 channels with signal");

    std::cout << "stereo output (original signal flow)" << std::endl;
    auto stereo = run(juce::AudioChannelSet::stereo(), 4.0, false);
    check(stereo.channelEnergy.size() == 2 && stereo.finite && stereo.channelEnergy[0] > 1.0, "stereo output with signal");
    check(std::abs(stereo.channelEnergy[0] - stereo.channelEnergy[1]) < 1.0e-3 * stereo.channelEnergy[0], "left and right match as before");

    std::cout << "generic 16 channel output (Reaper, standalone)" << std::endl;
    auto discrete = run(juce::AudioChannelSet::discreteChannels(16), 4.0, false);
    check(discrete.channelEnergy.size() == 16 && discrete.finite, "encoded as third order");
    double discreteHigherOrders = 0.0;
    for (size_t ch = 4; ch < discrete.channelEnergy.size(); ++ch) discreteHigherOrders += discrete.channelEnergy[ch];
    check(discreteHigherOrders > 0.0, "higher orders carry signal");

    std::cout << "AmbiX prepared, then switched to stereo without preparing again (Reaper)" << std::endl;
    auto switched = run(juce::AudioChannelSet::ambisonic(3), 4.0, false, juce::AudioChannelSet::stereo());
    check(switched.channelEnergy.size() == 2 && switched.finite && switched.channelEnergy[0] > 1.0, "stereo output with signal");
    check(std::abs(switched.channelEnergy[0] - switched.channelEnergy[1]) < 1.0e-3 * switched.channelEnergy[0],
          "left and right match (no W/Y on left/right)");

    std::cout << (failures == 0 ? "all checks passed" : juce::String(failures) + " check(s) failed") << std::endl;
    return failures == 0 ? 0 : 1;
}

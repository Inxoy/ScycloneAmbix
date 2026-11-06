// Plays a test signal through the plugin with every model of the Models folders in network I,
// in real time (the inference runs on its own thread), and reports the output level and how
// many blocks the model was late for.
//   ScycloneModelTest [seconds]

#include "PluginProcessor.h"
#include "PluginParameters.h"
#include "dsp/onnx/ModelLibrary.h"

namespace {
    void setParameter(juce::AudioProcessor& processor, const juce::ParameterID& id, float value) {
        for (auto* p : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
                if (ranged->getParameterID() == id.getParamID()) {
                    ranged->setValueNotifyingHost(ranged->convertTo0to1(value));
                    return;
                }
        std::cout << "parameter not found: " << id.getParamID() << std::endl;
    }

    float testSample(int n, double rate, juce::Random& random) {
        const double t = n / rate;
        float x = 0.15f * (float) std::sin(2.0 * juce::MathConstants<double>::pi * 110.0 * t);
        const int hit = n % (int) (rate / 4);                     // a decaying burst every 0.25 s
        x += 0.3f * std::exp(-hit / (float) (rate * 0.03)) * (random.nextFloat() * 2.f - 1.f);
        return x;
    }
}

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI gui;
    const double seconds = argc > 1 ? juce::String(argv[1]).getDoubleValue() : 6.0;
    const double rate = 48000.0;
    const int block = 512;

    const auto names = ModelLibrary::getNames();
    std::cout << "models: " << names.joinIntoString(", ") << std::endl;
    int failures = 0;

    for (int index = 0; index < names.size(); ++index) {
        AudioPluginAudioProcessor processor;
        processor.setPlayConfigDetails(2, 2, rate, block);
        processor.prepareToPlay(rate, block);
        processor.selectModel(1, index);
        setParameter(processor, PluginParameters::ON_OFF_NETWORK1_ID, 1.f);
        setParameter(processor, PluginParameters::ON_OFF_NETWORK2_ID, 0.f);
        setParameter(processor, PluginParameters::FADE_ID, 1.f);         // only network I
        setParameter(processor, PluginParameters::DRY_WET_ID, 1.f);      // only the wet signal
        setParameter(processor, PluginParameters::COMP_DRY_WET_ID, 0.f); // no compressor

        const int latency = processor.getLatencySamples();
        juce::AudioBuffer<float> buffer(2, block);
        juce::MidiBuffer midi;
        juce::Random random(1);
        double sum = 0.0;
        int counted = 0, silentBlocks = 0, n = 0;
        const int total = (int) (seconds * rate);
        auto start = juce::Time::getMillisecondCounterHiRes();

        for (int done = 0; done < total; done += block) {
            for (int i = 0; i < block; ++i) {
                const float x = testSample(n++, rate, random);
                buffer.setSample(0, i, x);
                buffer.setSample(1, i, x);
            }
            processor.processBlock(buffer, midi);

            // after the latency and a second of settling: level and silent (late) blocks
            if (done > latency + (int) rate) {
                const float level = buffer.getRMSLevel(0, 0, block);
                sum += (double) level * level;
                ++counted;
                // a late model leaves exact zeros (quiet model output is not counted)
                if (buffer.findMinMax(0, 0, block).isEmpty() && buffer.getSample(0, 0) == 0.f) {
                    if (silentBlocks < 12) std::cout << "  late block at " << (done / rate) << " s" << std::endl;
                    ++silentBlocks;
                }
            }
            // real time: the inference thread has as long as it would in a host
            const double due = start + 1000.0 * (done + block) / rate;
            const double now = juce::Time::getMillisecondCounterHiRes();
            if (due > now) juce::Thread::sleep((int) (due - now));
        }
        processor.releaseResources();

        const double rms = counted > 0 ? std::sqrt(sum / counted) : 0.0;
        const bool ok = rms > 1.0e-4 && std::isfinite(rms) && silentBlocks == 0;
        if (!ok) ++failures;
        std::cout << (ok ? "ok    " : "FAIL  ") << names[index] << ": output rms " << rms
                  << ", late blocks " << silentBlocks << " of " << counted
                  << ", now running " << processor.getModelName(1) << std::endl;
    }
    return failures;
}

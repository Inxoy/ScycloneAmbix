# <img style="float: left;" src="assets/pictures/logo.png" width="40" /> &nbsp; SCYCLONE
![interface](assets/pictures/interface.png)

### Winner of the 2023 Neural Audio Plugin Competition, hosted by The Audio Programmer

> Scyclone stood out because it took cutting-edge neural audio research and turned it into a plugin musicians actually wanted to use. It's exactly the kind of work we hoped the competition would bring out.
>
> — The Audio Programmer

**Scyclone** is an audio plugin that utilizes **neural timbre transfer** technology to offer a new approach to audio production. The plugin builds upon [RAVE](https://github.com/acids-ircam/RAVE) methodology, a realtime audio variational auto encoder, facilitating neural timbre transfer in both single and couple inference mode. <br /><br />
This enables a new artificial layering technique to be applied on the incoming signal in creating richer drum pallets, fuller atmospheres or simply transferring the timbre of the raw signal to another sound pallet. To further control the behaviour and production of the neural networks, we have internally equipped the plugin with signal processings modules allowing the user to shape, control and embellish the source and target timbres in a distinct manner.

## Overview
![signal_flow](assets/pictures/signal_flow_control.png)



**Signal flow**: <br />

Scyclone offers an intuitive signal flow allowing for a seamless influence over inference and sound synthesis. The pre-processing modules are:

- Transient Controller: Shaping the attack and sustain of the singal
- Low-/High-Cut Filter: Refining the frequency range of input audio favouring a consistent sound
 
Additional in-built postprocessing modules permit for further manipulation and formation of the timbre transferred signal. The post-processing modules are:
 
- Grain Delay: Adding depth and texture
- Blend: Crossfades between the outputs of the models and obtain a harmonious mix
- Post-Compressor: Controls the dynamics and glues the outputs together

**Trained models**:<br />

We have provided two pre-trained models (presets) accessible under **assets/models** directory.

- **Funk Drums**: Trained on four hours of captivating vintage drum-breaks
- **Djembe**: Trained on five hours of carefully compiled Djembe dataset (Three hours of Djembe solo performances and two hours of one-shot recordings)

## ScycloneAmbix: latent space as ambisonic parameters

This fork outputs **AmbiX** (ACN channel order, SN3D normalisation) of first to third order. The plugin defaults to 16 channels (third order). Hosts can also choose 4 or 9 channels, or plain mono/stereo, which keeps the original signal flow.

Each network's output is encoded as its own point source. Its position is steered by the network's **RAVE latent space**:

- Every network has a base **Azimuth**, **Elevation** and **Spread**.
- One latent dimension (selectable, `Latent 1`–`Latent 16`) modulates each of the three, scaled by a **Latent Depth** (100 % = ±180° azimuth, ±90° elevation, ±1 spread).
- Latent values are normalised to [-1, 1] by a running mean and standard deviation (about 8 s), because their ranges differ a lot between models.
- **Ambi Smoothing** sets the time constant of the movement. The direction is smoothed as a vector, so there are no jumps at ±180°.
- **Spread** fades out the higher orders. At 100 % only W remains, so the source is omnidirectional.
- The **Fade** parameter sets the gains of the two sources. The compressor detects on W, which is the mono mix of both networks, and applies the same gain to all channels.
- The dry signal is encoded as a source pair at ±**Ambi Dry Width** (stereo input) or straight ahead (mono input), so **Global Mix** blends two ambisonic signals.

All parameters are automatable. Their names start with `Ambi`.

The bundled models expose the latent as a second output. The plugin reads one latent frame per model hop (2048 samples for the bundled models) and keeps it aligned with the delayed network audio. To use your own RAVE export, convert it with [`tools/add_latent_output.py`](tools/add_latent_output.py) (instructions are in the script). Models without a latent output still work: the sources then stay at their base positions.

Offline checks of the encoding (SN3D norms, latent modulation, stereo fallback) run with:

```bash
cmake . -B cmake-build -DSCYCLONE_BUILD_TESTS=ON
cmake --build cmake-build --config Release --target ScycloneAmbixTest
cmake-build/ScycloneAmbixTest_artefacts/Release/ScycloneAmbixTest
```

## Installation

Detailed instructions can be found here:
- [Windows Guide](docs/install_instructions_windows.md).
- [Mac Guide](docs/install_instructions_mac.md).

## Build instruction

Build with CMake
```bash
# clone the repository
git clone https://github.com/Torsion-Audio/Scyclone
cd Scyclone/

# initialize and set up submodules
git submodule update --init --recursive

# on macOS you might need to specify the processor type with -DCMAKE_HOST_SYSTEM_PROCESSOR=x86_64 or arm64
# on Windows currently only Release build works (add your own onnx static debug build in order to use debug)
cmake . -B cmake-build
cmake --build cmake-build --config Release
```

**Notes:** 
- The onnx library is now linked statically. No more need to download the onnx library via homebrew or via the github repository. Also the binaries are now notarized.
- For Windows at the moment only release builds are supported. Debug builds will be supported with future updates.
- The AU plugin has not been tested with Logic yet. Logic support will come in futher updates.

## Training
Discover how to train your own RAVE-Models for Scyclone with our comprehensive step-by-step guide: 
- [Scyclone-AI](https://github.com/Torsion-Audio/Scyclone-AI/)

## References

- RAVE Paper - [RAVE: A variational autoencoder for fast and high-quality neural audio synthesis](https://arxiv.org/abs/2111.05011)
- RAVE Scripts - [RAVE Github Repository](https://github.com/acids-ircam/RAVE)
- RNBO Tutorial - [JUCE & RNBO C++ Export](https://kengo.dev/posts/jr-granular)
- ONNX static library builder - [ort-builder](https://github.com/olilarkin/ort-builder)

## Licenses
This project is subject to multiple licenses. The primary license for the entire project is the GNU General Public License version 3 (GPLv3), which is the most restrictive of all the licenses applied herein.
 - The Granular Delay module located at ```modules/RnboExport/``` is licensed under the [GPLv3](https://support.cycling74.com/hc/en-us/articles/10730637742483-RNBO-Export-Licensing-FAQ)
 - All pretrained onnx models located at ```assets/models/``` are licensed under the [Creative Commons Attribution-NonCommercial 4.0 International License](https://github.com/acids-ircam/RAVE/blob/master/LICENSE) 
 - All other code within this project is licensed under the MIT License.

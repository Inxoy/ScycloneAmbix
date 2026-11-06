# RAVE models in ScycloneAmbix

Besides the two built-in networks (Funk, Djembe), each network can run RAVE models exported
from TorchScript (`.ts`), e.g. the IRCAM models at
https://play.forum.ircam.fr/rave-vst-api/get_available_models (CC BY-NC 4.0: non-commercial use).

## 1. A full onnxruntime (Windows)

The library in `modules/onnxruntime` only has the operators of the built-in models. RAVE
models need a full build:

    tools\build_onnxruntime_windows.bat path\to\python.exe

This builds onnxruntime 1.14.1 (~30 min) and writes
`modules/onnxruntime-full/lib/onnxruntime-win-x64.lib` (~570 MB, not in git). CMake uses it
when it is there ("onnxruntime: full build" in the configure output). macOS still links the
reduced libraries.

## 2. Converting a model

    py -3.10 -m venv venv
    venv\Scripts\pip install torch==2.1.2 --index-url https://download.pytorch.org/whl/cpu
    venv\Scripts\pip install "onnx<1.15" onnxruntime==1.14.1 "numpy<2" flatbuffers
    venv\Scripts\python tools\convert_rave_ts.py percussion.ts out\
    venv\Scripts\python tools\check_rave_onnx.py percussion.ts out\percussion.onnx

The converted model streams like the original: the convolution caches are extra inputs and
outputs that the plugin carries from one chunk (8192 samples at the model's rate) to the
next; the plugin resamples to and from the model's rate. The check compares it sample by
sample with the TorchScript model (noise switched off on both sides) and measures the
inference time per chunk, which has to stay below the chunk's duration.

## 3. Installing

Put the `.ort` file in one of these folders; the file name becomes the name in the model menu
(`sol_ordinario.ort` -> "Sol Ordinario"):

- `%APPDATA%\ScycloneAmbix\Models` (Windows), `~/Library/ScycloneAmbix/Models` (macOS)
- `Models` next to the standalone app
- `Contents/Resources/Models` inside the VST3 bundle

## The IRCAM models

| model          | check                         | time per 171-186 ms chunk |
|----------------|-------------------------------|---------------------------|
| percussion     | matches (-101 dB)             | 34 ms                     |
| isis           | matches (-97 dB)              | 22 ms                     |
| sol_ordinario  | matches (-112 dB)             | 23 ms                     |
| nasa           | differs (-35 dB), runs        | 16 ms                     |
| VCTK           | differs (-20 dB), runs        | 36 ms                     |
| vintage        | matches (-97 dB), too slow    | 303 ms                    |

Measured on the development machine. nasa and VCTK run but don't match their originals
exactly yet; vintage (RAVE v1 large) needs more CPU than real time allows here.

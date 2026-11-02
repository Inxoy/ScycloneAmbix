"""
Adds the RAVE latent as a second output to an exported Scyclone ONNX model, so
ScycloneAmbix can use it to steer the ambisonic encoding.

The latent mean (encoder output before the reparametrisation noise) is exposed as
output 1 with shape [1, latent_dimensions, frames]. The audio output stays output 0,
so the model still works in the original Scyclone.

The plugin links onnxruntime 1.14.1 (reduced operator build), so the .ort file must be
written by the same version. Python 3.10 works with it:

    python -m venv venv
    venv/Scripts/pip install onnxruntime==1.14.1 "onnx<1.15" "numpy<2" flatbuffers
    venv/Scripts/python tools/add_latent_output.py model.onnx out_dir/

This writes out_dir/model.onnx and out_dir/model.ort.
"""

import argparse
import pathlib
import subprocess
import sys

import onnx


def find_latent_tensor(graph: onnx.GraphProto) -> str:
    # RAVE samples z = mean + std * randn_like(mean), where mean is a Slice of the encoder
    # output, so the input of that RandomNormalLike node is the latent mean. The decoder's
    # noise generator may contain further RandomNormalLike nodes.
    producers = {out: node for node in graph.node for out in node.output}
    noise_nodes = [n for n in graph.node if n.op_type == "RandomNormalLike"
                   and n.input[0] in producers and producers[n.input[0]].op_type == "Slice"]
    if len(noise_nodes) != 1:
        raise RuntimeError(f"expected one RandomNormalLike node, found {len(noise_nodes)}. Is this a RAVE export?")
    return noise_nodes[0].input[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("model", type=pathlib.Path)
    parser.add_argument("out_dir", type=pathlib.Path)
    args = parser.parse_args()

    model = onnx.load(str(args.model))
    if len(model.graph.output) > 1:
        print("model already has more than one output, nothing to do")
        return 0

    latent = find_latent_tensor(model.graph)
    print(f"latent tensor: {latent}")
    # exposed directly, the reduced onnxruntime build has no Identity kernel
    model.graph.output.append(onnx.helper.make_tensor_value_info(latent, onnx.TensorProto.FLOAT, None))

    args.out_dir.mkdir(parents=True, exist_ok=True)
    out_model = args.out_dir / args.model.name
    onnx.save(model, str(out_model))

    subprocess.check_call([sys.executable, "-m", "onnxruntime.tools.convert_onnx_models_to_ort", str(out_model),
                           "--output_dir", str(args.out_dir)])
    print(f"wrote {out_model} and {out_model.with_suffix('.ort')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

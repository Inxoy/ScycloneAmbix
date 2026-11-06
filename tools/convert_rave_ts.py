"""
Converts a RAVE TorchScript export (.ts, e.g. the models from
https://play.forum.ircam.fr/rave-vst-api/get_available_models) to an ONNX / ORT model
that ScycloneAmbix can load.

RAVE .ts files are streaming exports: every convolution keeps a cache of the last samples
it saw. The exported ONNX graph keeps that streaming behaviour, with the caches as extra
inputs and outputs; the plugin feeds each call the states the previous call returned.

Inputs / outputs of the ONNX model (all at the model's own sample rate):
    input              [1, 1, chunk]          chunk = --chunk, a multiple of the model's ratio
    state_<n>          the cache of convolution n before the chunk (zeros at the start)
    audio              [1, 1, chunk]          stereo models are summed to mono
    latent             [1, dims, chunk / ratio]   the RAVE latent (for the ambisonic steering)
    new_state_<n>      the caches after the chunk

The sample rate is stored in the model's metadata ("sample_rate"), the plugin resamples
to and from it.

The plugin needs a full onnxruntime build (DFT, Cos, RandomUniformLike, ... are used).

    py -3.10 -m venv venv
    venv/Scripts/pip install torch==2.1.2 --index-url https://download.pytorch.org/whl/cpu
    venv/Scripts/pip install "onnx<1.15" onnxruntime==1.14.1 "numpy<2" flatbuffers
    venv/Scripts/python tools/convert_rave_ts.py percussion.ts out_dir/

This writes out_dir/percussion.onnx and out_dir/percussion.ort. tools/check_rave_onnx.py
compares the result with the TorchScript model.
"""

import argparse
import pathlib
import subprocess
import sys

import numpy as np
import onnx
import torch

DEFAULT_CHUNK = 8192


def model_rate(model) -> int:
    for name in ("sr", "sampling_rate"):
        if hasattr(model, name):
            return int(getattr(model, name))
    return 48000


def model_ratio(model) -> int:
    """samples per latent frame"""
    return int(model.encode_params[3])


class StreamingRave(torch.nn.Module):
    """scripted (not traced): tracing can't call encode/decode of a TorchScript model"""

    def __init__(self, model):
        super().__init__()
        self.model = model

    def forward(self, x: torch.Tensor):
        z = self.model.encode(x)
        y = self.model.decode(z)
        return y.mean(dim=1, keepdim=True), z


#
# ONNX has no complex tensors. RAVE v1's noise generator builds its filters with
# torch.fft (irfft of a real spectrum, then an FFT convolution). Complex values are kept
# here as a trailing dimension of 2 (real, imaginary), the layout of ONNX's DFT operator.
#
def _is_complex(value) -> bool:
    try:
        return value.type().scalarType() in ("ComplexFloat", "ComplexDouble")
    except RuntimeError:
        return False


def _const_i64(g, values):
    return g.op("Constant", value_t=torch.tensor(values, dtype=torch.int64))


def _real_imag(g, x):
    axis = _const_i64(g, [-1])
    real = g.op("Squeeze", g.op("Slice", x, _const_i64(g, [0]), _const_i64(g, [1]), axis), axis)
    imag = g.op("Squeeze", g.op("Slice", x, _const_i64(g, [1]), _const_i64(g, [2]), axis), axis)
    return real, imag


def _complex(g, real, imag):
    axis = _const_i64(g, [-1])
    return g.op("Concat", g.op("Unsqueeze", real, axis), g.op("Unsqueeze", imag, axis), axis_i=-1)


def _check_last_dim(sym_help, dim, what):
    d = sym_help._maybe_get_const(dim, "i")
    if d not in (-1,) and not (isinstance(d, int) and d < 0 and d == -1):
        raise RuntimeError(f"{what}: only dim=-1 is supported, got {d}")


def register_complex_symbolics(opset: int):
    from torch.onnx import register_custom_op_symbolic, symbolic_helper as sh
    from torch.onnx import symbolic_opset9 as op9

    def view_as_complex(g, x):
        return x

    def view_as_real(g, x):
        return x

    def fft_rfft(g, x, n, dim, norm):
        if not sh._is_none(n) or not sh._is_none(norm):
            raise RuntimeError("fft_rfft: n and norm are not supported")
        _check_last_dim(sh, dim, "fft_rfft")
        signal = g.op("Unsqueeze", x, _const_i64(g, [-1]))
        return g.op("DFT", signal, onesided_i=1, inverse_i=0, axis_i=-2)

    def fft_irfft(g, x, n, dim, norm):
        if not sh._is_none(n) or not sh._is_none(norm):
            raise RuntimeError("fft_irfft: n and norm are not supported")
        _check_last_dim(sh, dim, "fft_irfft")
        # full spectrum from the half spectrum: X[N-k] = conj(X[k]) for k = 1 .. F-2
        mirrored = g.op("Slice", x, _const_i64(g, [-2]), _const_i64(g, [0]), _const_i64(g, [-2]), _const_i64(g, [-1]))
        conj = g.op("Mul", mirrored, g.op("Constant", value_t=torch.tensor([1.0, -1.0])))
        full = g.op("Concat", x, conj, axis_i=-2)
        time = g.op("DFT", full, onesided_i=0, inverse_i=1, axis_i=-2)
        real, _ = _real_imag(g, time)
        return real

    def mul(g, a, b):
        if _is_complex(a) and _is_complex(b):
            ar, ai = _real_imag(g, a)
            br, bi = _real_imag(g, b)
            return _complex(g, g.op("Sub", g.op("Mul", ar, br), g.op("Mul", ai, bi)),
                            g.op("Add", g.op("Mul", ar, bi), g.op("Mul", ai, br)))
        return op9.mul(g, a, b)

    def hann_window(g, length, periodic=None, dtype=None, layout=None, device=None, pin_memory=None):
        # periodic Hann window: 0.5 - 0.5 cos(2 pi n / N)
        n_float = g.op("Cast", length, to_i=1)
        n = g.op("Range", g.op("Constant", value_t=torch.tensor(0.0)), n_float, g.op("Constant", value_t=torch.tensor(1.0)))
        phase = g.op("Div", g.op("Mul", n, g.op("Constant", value_t=torch.tensor(2.0 * np.pi, dtype=torch.float32))), n_float)
        half = g.op("Constant", value_t=torch.tensor(0.5))
        return g.op("Sub", half, g.op("Mul", half, g.op("Cos", phase)))

    def repeat_interleave(g, x, repeats, dim=None, output_size=None):
        # the built-in version needs the tensor rank, which scripted graphs often don't carry
        if dim is None or sh._is_none(dim):
            # no dim: torch flattens the input first
            x = g.op("Reshape", x, _const_i64(g, [-1]))
            d = 0
        else:
            d = sh._maybe_get_const(dim, "i")
        if not isinstance(d, int):
            raise RuntimeError(f"repeat_interleave: only a constant dim is supported, got {dim.node()} / repeats {repeats.node()}")
        r = sh._maybe_get_const(repeats, "i")
        if isinstance(r, int):
            count = _const_i64(g, [r])
        else:   # computed at run time
            count = g.op("Reshape", g.op("Cast", repeats, to_i=7), _const_i64(g, [1]))
        big = 2 ** 62

        def parts(shape, axis):
            """shape[:axis] and shape[axis + 1:] for a constant axis, negative or not"""
            before = g.op("Slice", shape, _const_i64(g, [0]), _const_i64(g, [axis])) if axis != 0 else _const_i64(g, [])
            if axis == -1:
                after = _const_i64(g, [])
            else:
                after = g.op("Slice", shape, _const_i64(g, [axis + 1]), _const_i64(g, [big]))
            return before, after

        # x[..., n, ...] -> x[..., n, 1, ...] -> expand to x[..., n, r, ...] -> x[..., n * r, ...]
        new_axis = d if d < 0 else d + 1
        copies = g.op("Unsqueeze", x, _const_i64(g, [new_axis]))
        before, after = parts(g.op("Shape", copies), new_axis)
        copies = g.op("Expand", copies, g.op("Concat", before, count, after, axis_i=0))
        before, after = parts(g.op("Shape", x), d)
        return g.op("Reshape", copies, g.op("Concat", before, _const_i64(g, [-1]), after, axis_i=0))

    register_custom_op_symbolic("aten::repeat_interleave", repeat_interleave, opset)
    register_custom_op_symbolic("aten::view_as_complex", view_as_complex, opset)
    register_custom_op_symbolic("aten::view_as_real", view_as_real, opset)
    register_custom_op_symbolic("aten::fft_rfft", fft_rfft, opset)
    register_custom_op_symbolic("aten::fft_irfft", fft_irfft, opset)
    register_custom_op_symbolic("aten::mul", mul, opset)
    register_custom_op_symbolic("aten::hann_window", hann_window, opset)


def reset_streaming_state(model):
    """the caches start from silence"""
    with torch.no_grad():
        for name, buffer in model.named_buffers():
            if name.endswith(".pad"):
                buffer.zero_()


def _attribute_path(value):
    """'model.encoder.net.0.cache.pad' for a value read with a chain of prim::GetAttr, else None"""
    parts = []
    node = value.node()
    while node.kind() == "prim::GetAttr":
        parts.append(node.s("name"))
        node = node.inputsAt(0).node()
    if not parts or node.kind() != "prim::Param":
        return None
    return ".".join(reversed(parts))


def _attribute_path_through_batch_slice(value):
    """newer cached_conv versions keep caches for up to 64 streams and update pad[:batch]"""
    node = value.node()
    if node.kind() == "aten::slice":
        dim = node.inputsAt(1)
        if dim.node().kind() == "prim::Constant" and dim.toIValue() == 0:
            return _attribute_path(node.inputsAt(0))
    return _attribute_path(value)


def _resolve(module, path):
    for part in path.split("."):
        module = getattr(module, part)
    return module


def functionalize_slice_adds(graph) -> int:
    """`y[..., :n] += cache` (the overlap-add of the cached transposed convolutions) gets
    lost in the ONNX export; it is rewritten as y = cat([y[..., :n] + cache, y[..., n:]])"""
    count = 0
    for node in list(graph.nodes()):
        if node.kind() != "aten::add_":
            continue
        target = node.inputsAt(0).node()
        if target.kind() != "aten::slice":
            continue
        y, dim, start, end, step = target.inputs()
        start_value = start.toIValue() if start.node().kind() == "prim::Constant" else "dynamic"
        if start_value not in (None, 0):
            raise RuntimeError("in-place add on a slice that doesn't start at 0, not supported")
        with graph.insert_point_guard(node):
            one = graph.insertConstant(1)
            none = graph.insertConstant(None)
            head = graph.insertNode(graph.create("aten::slice", [y, dim, start, end, step])).output()
            summed = graph.insertNode(graph.create("aten::add", [head, node.inputsAt(1), one])).output()
            tail = graph.insertNode(graph.create("aten::slice", [y, dim, end, none, step])).output()
            parts = graph.insertNode(graph.create("prim::ListConstruct", [summed, tail])).output()
            parts.setType(torch._C.ListType.ofTensors())
            joined = graph.insertNode(graph.create("aten::cat", [parts, dim])).output()
        for v in (head, summed, tail, joined):
            v.setType(y.type())
        y.replaceAllUsesAfterNodeWith(joined.node(), joined)
        node.output().replaceAllUsesWith(summed)
        node.destroy()
        count += 1
    torch._C._jit_pass_dce(graph)
    return count


def _cache_updates(graph):
    """[(node, path)] of every cache update: copy_ into a cache (possibly through a batch
    slice) or, in older versions, an assignment self.pad = ..."""
    updates = []

    def collect(block):
        for node in block.nodes():
            for sub in node.blocks():
                collect(sub)
            if node.kind() == "aten::copy_":
                path = _attribute_path_through_batch_slice(node.inputsAt(0))
                if path is not None:
                    updates.append((node, path))
            elif node.kind() == "prim::SetAttr" and node.s("name") in ("pad", "cache"):
                parent = _attribute_path(node.inputsAt(0))
                parent = "" if parent is None and node.inputsAt(0).node().kind() == "prim::Param" else parent
                if parent is not None:
                    updates.append((node, (parent + "." if parent else "") + node.s("name")))

    collect(graph)
    return updates


def cache_shapes(module, example):
    """the shape each cache really has in use: caches kept for up to 64 streams are used with
    the batch of the call (2 in the decoder of a stereo model). Runs the graph once with
    the updates as extra outputs."""
    graph = module.graph.copy()
    updates = _cache_updates(graph)
    # functions return one value: (the original result, update 1, update 2, ...)
    values = [next(graph.outputs())] + [node.inputsAt(1) for node, _ in updates]
    with graph.insert_point_guard(graph.return_node()):
        packed = graph.insertNode(graph.create("prim::TupleConstruct", values)).output()
    packed.setType(torch._C.TupleType([v.type() for v in values]))
    graph.eraseOutput(0)
    graph.registerOutput(packed)
    probe = torch._C._create_function_from_graph("probe_caches", graph)
    with torch.no_grad():
        outputs = probe(module._c, example)
    shapes = {}
    for (_, path), value in zip(updates, outputs[1:]):
        shapes[path] = tuple(value.shape)
    return shapes


def make_stateful(graph, module, shapes=None):
    """turns the cache buffers into graph inputs and their updates into graph outputs.

    Every cache follows `x = cat([pad, x]); pad.copy_(x[..., -padding:])`: reads of `pad`
    become the input `state_<n>`, the source of the copy the output `new_state_<n>`.
    Returns [(name, initial value)] in input order."""
    copies = []

    def collect(block, depth):
        for node in block.nodes():
            for sub in node.blocks():
                collect(sub, depth + 1)
            if node.kind() == "aten::copy_":
                path = _attribute_path_through_batch_slice(node.inputsAt(0))
                if path is not None:
                    copies.append((node, path, depth))
            elif node.kind() == "prim::SetAttr" and node.s("name") in ("pad", "cache"):
                # older cached_conv versions assign the cache: self.pad = x[..., -padding:]
                parent = _attribute_path(node.inputsAt(0))
                parent = "" if parent is None and node.inputsAt(0).node().kind() == "prim::Param" else parent
                if parent is not None:
                    copies.append((node, (parent + "." if parent else "") + node.s("name"), depth))

    collect(graph, 0)
    nested = [p for _, p, d in copies if d > 0]
    if nested:
        raise RuntimeError(f"cache updates inside control flow, not supported: {nested[:3]}")

    # all reads of each cache, anywhere in the graph
    reads = {}

    def collect_reads(block):
        for node in block.nodes():
            for sub in node.blocks():
                collect_reads(sub)
            if node.kind() == "prim::GetAttr":
                path = _attribute_path(node.output())
                if path is not None:
                    reads.setdefault(path, []).append(node.output())

    collect_reads(graph)

    states = []
    for node, path, _ in copies:
        # the shape the cache has in use (see cache_shapes), else the batch-1 part
        full = _resolve(module, path)
        if shapes and path in shapes:
            buffer = torch.zeros(shapes[path], dtype=full.dtype)
        else:
            buffer = full[:1]
        source = node.inputsAt(1)
        if node.kind() == "aten::copy_":
            for out in node.outputs():
                out.replaceAllUsesWith(node.inputsAt(0))
        node.destroy()
        if buffer.numel() == 0:
            continue            # zero sized delays: nothing to carry
        name = f"state_{len(states)}"
        if not reads.get(path):
            print(f"  warning: no read of {path} found, its state can't be carried")
        value = graph.addInput(name)
        value.setType(torch._C.TensorType.create_from_tensor(buffer))
        for read in reads.get(path, []):
            read.replaceAllUsesWith(value)
        # state[:batch] is the state itself with one stream; the dynamic end (x.shape[0])
        # would otherwise reach the exporter without a known rank
        for use in list(value.uses()):
            user = use.user
            if user.kind() == "aten::slice" and use.offset == 0:
                dim = user.inputsAt(1)
                if dim.node().kind() == "prim::Constant" and dim.toIValue() == 0:
                    user.output().replaceAllUsesWith(value)
        graph.registerOutput(source)
        states.append((name, torch.zeros_like(buffer)))

    # what is left: other attribute writes (initialisation flags), not needed
    def strip(block):
        for node in list(block.nodes()):
            for sub in node.blocks():
                strip(sub)
            if node.kind() == "prim::SetAttr":
                node.destroy()

    strip(graph)
    torch._C._jit_pass_dce(graph)
    return states


def ops_of(model: onnx.ModelProto) -> set:
    found = set()

    def walk(graph):
        for node in graph.node:
            found.add((node.domain or "ai.onnx", node.op_type))
            for attr in node.attribute:
                if attr.g is not None and attr.g.node:
                    walk(attr.g)
                for g in attr.graphs:
                    walk(g)

    walk(model.graph)
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("model", type=pathlib.Path)
    parser.add_argument("out_dir", type=pathlib.Path)
    parser.add_argument("--chunk", type=int, default=DEFAULT_CHUNK, help="samples per call at the model's rate")
    parser.add_argument("--opset", type=int, default=17, help="17 is the first opset with DFT")
    parser.add_argument("--name", help="file name of the output (default: the input's)")
    args = parser.parse_args()

    register_complex_symbolics(args.opset)

    torch.manual_seed(0)
    ts = torch.jit.load(str(args.model), map_location="cpu").eval()
    rate, ratio = model_rate(ts), model_ratio(ts)
    if args.chunk % ratio:
        print(f"--chunk must be a multiple of the model's ratio ({ratio})")
        return 1
    # one call so the caches are initialised (allocated), then back to silence
    with torch.no_grad():
        ts.decode(ts.encode(torch.zeros(1, 1, args.chunk)))
    reset_streaming_state(ts)

    # frozen: module attributes (ratios, sizes) become constants the exporter can use
    wrapper = torch.jit.freeze(torch.jit.script(StreamingRave(ts).eval()), preserved_attrs=[])
    shapes = cache_shapes(wrapper, torch.zeros(1, 1, args.chunk))
    reset_streaming_state(ts)
    overlap_adds = functionalize_slice_adds(wrapper.graph)
    states = make_stateful(wrapper.graph, wrapper, shapes)
    print(f"{args.model.name}: {rate} Hz, ratio {ratio}, chunk {args.chunk}, {len(states)} caches, "
          f"{overlap_adds} overlap-adds rewritten")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    stem = args.name or args.model.stem
    out_model = args.out_dir / f"{stem}.onnx"
    example = (torch.zeros(1, 1, args.chunk),) + tuple(s for _, s in states)
    names = [n for n, _ in states]
    # the exporter runs the module once for the structure of its outputs, but the method's
    # signature still has the single input of before make_stateful; hand it the structure
    import torch.onnx.utils as onnx_utils
    with torch.no_grad():
        probe_z = ts.encode(torch.zeros(1, 1, args.chunk))
    reset_streaming_state(ts)
    example_out = ((torch.zeros(1, 1, args.chunk), torch.zeros_like(probe_z)),) + tuple(s for _, s in states)
    onnx_utils._get_example_outputs = lambda model, a: example_out
    with torch.no_grad():
        torch.onnx.export(wrapper, example, str(out_model), opset_version=args.opset,
                          input_names=["input"] + names,
                          output_names=["audio", "latent"] + ["new_" + n for n in names],
                          do_constant_folding=True)

    exported = onnx.load(str(out_model))
    onnx.helper.set_model_props(exported, {"sample_rate": str(rate), "ratio": str(ratio), "streaming": "1"})
    onnx.save(exported, str(out_model))
    print("operators:", ", ".join(sorted(op for _, op in ops_of(exported))))

    # quick sanity run with onnxruntime: two chunks, the states carried over
    import onnxruntime as ort
    session = ort.InferenceSession(str(out_model), providers=["CPUExecutionProvider"])
    t = np.arange(2 * args.chunk, dtype=np.float32) / rate
    test = (0.3 * np.sin(2 * np.pi * 220 * t)).astype(np.float32)
    # some caches turn out not to matter and their inputs are dropped: go by name
    inputs = {i.name for i in session.get_inputs()}
    output_names = [o.name for o in session.get_outputs()]
    feed = {n: s.numpy() for n, s in states if n in inputs}
    for i in range(2):
        feed["input"] = test[None, None, i * args.chunk:(i + 1) * args.chunk]
        outputs = dict(zip(output_names, session.run(None, feed)))
        for n in list(feed):
            if n != "input":
                feed[n] = outputs["new_" + n]
    audio, latent = outputs["audio"], outputs["latent"]
    print(f"audio {audio.shape} rms {float(np.sqrt(np.mean(audio ** 2))):.4f}, latent {latent.shape}")
    if audio.shape != (1, 1, args.chunk) or not np.isfinite(audio).all():
        print("unexpected output")
        return 3

    subprocess.check_call([sys.executable, "-m", "onnxruntime.tools.convert_onnx_models_to_ort", str(out_model),
                           "--output_dir", str(args.out_dir)])
    print(f"wrote {out_model} and {out_model.with_suffix('.ort')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

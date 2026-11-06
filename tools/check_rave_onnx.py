"""
Compares a model converted with convert_rave_ts.py with its TorchScript original.

Both stream the same signal at the model's sample rate: the ONNX model chunk by chunk
with its caches carried over (as ScycloneAmbix runs it), the TorchScript model in its own
streaming mode. RAVE draws random noise; for the comparison it is replaced by zeros on
both sides, so the outputs have to match sample by sample.

    venv/Scripts/python tools/check_rave_onnx.py model.ts model.onnx
"""

import sys
import time

import numpy as np
import onnx
import onnxruntime as ort
import torch


def test_signal(rate, seconds=6.0, seed=1):
    rng = np.random.default_rng(seed)
    n = int(seconds * rate)
    t = np.arange(n) / rate
    x = 0.15 * np.sin(2 * np.pi * 110 * t) * (0.5 + 0.5 * np.sin(2 * np.pi * 0.5 * t))
    for start in range(0, n, rate // 4):          # decaying hits every 0.25 s
        length = min(n - start, rate // 6)
        env = np.exp(-np.arange(length) / (rate * 0.03))
        x[start:start + length] += 0.3 * env * rng.standard_normal(length)
    return x.astype(np.float32)


def deterministic_onnx(model):
    zero_name = "check_zero"
    model.graph.initializer.append(onnx.helper.make_tensor(zero_name, onnx.TensorProto.FLOAT, [], [0.0]))
    nodes = []
    for node in model.graph.node:
        if node.op_type in ("RandomNormalLike", "RandomUniformLike"):
            nodes.append(onnx.helper.make_node("Mul", [node.input[0], zero_name], list(node.output)))
        elif node.op_type in ("RandomNormal", "RandomUniform"):
            raise RuntimeError("RandomNormal/RandomUniform with a fixed shape: not handled by this check")
        else:
            nodes.append(node)
    del model.graph.node[:]
    model.graph.node.extend(nodes)
    return model


def deterministic_torchscript(m):
    for method in (m.encode, m.decode):
        graph = method.graph
        torch._C._jit_pass_inline(graph)

        def walk(block):
            for node in list(block.nodes()):
                for sub in node.blocks():
                    walk(sub)
                if node.kind() in ("aten::randn_like", "aten::rand_like"):
                    with graph.insert_point_guard(node):
                        zero = graph.insertConstant(0.0)
                        mul = graph.create("aten::mul", [node.inputsAt(0), zero])
                        graph.insertNode(mul)
                    node.output().replaceAllUsesWith(mul.output())
                    node.destroy()
                elif node.kind() == "aten::randn":
                    # same arguments as aten::zeros (size, dtype, layout, device, pin_memory)
                    with graph.insert_point_guard(node):
                        zeros = graph.create("aten::zeros", list(node.inputs()))
                        graph.insertNode(zeros)
                    node.output().replaceAllUsesWith(zeros.output())
                    node.destroy()

        walk(graph)


def run_onnx(model_proto, x, deterministic):
    if deterministic:
        model_proto = deterministic_onnx(model_proto)
    session = ort.InferenceSession(model_proto.SerializeToString(), providers=["CPUExecutionProvider"])
    chunk = session.get_inputs()[0].shape[2]
    feed = {i.name: np.zeros([d if isinstance(d, int) else 1 for d in i.shape], dtype=np.float32)
            for i in session.get_inputs()[1:]}
    names = [o.name for o in session.get_outputs()]
    out, seconds = [], 0.0
    for i in range(len(x) // chunk):
        feed["input"] = x[None, None, i * chunk:(i + 1) * chunk]
        start = time.perf_counter()
        result = dict(zip(names, session.run(None, feed)))
        seconds += time.perf_counter() - start
        out.append(result["audio"][0, 0])
        for n in list(feed):
            if n != "input":
                feed[n] = result["new_" + n]
    return np.concatenate(out), seconds / max(1, len(out)), chunk


def run_torchscript(path, x, deterministic, block=2048):
    m = torch.jit.load(path, map_location="cpu").eval()
    if deterministic:
        deterministic_torchscript(m)
    signal = torch.from_numpy(x)[None, None, :]
    usable = (signal.shape[-1] // block) * block
    with torch.no_grad():
        y = torch.cat([m.decode(m.encode(signal[..., i:i + block])) for i in range(0, usable, block)], dim=-1)
    return y.mean(dim=1)[0].numpy()


def main():
    ts_path, onnx_path = sys.argv[1], sys.argv[2]
    model = onnx.load(onnx_path)
    props = {p.key: p.value for p in model.metadata_props}
    rate = int(props.get("sample_rate", 48000))
    torch.manual_seed(0)
    x = test_signal(rate)

    a, per_call, chunk = run_onnx(onnx.load(onnx_path), x, deterministic=True)
    b = run_torchscript(ts_path, x, deterministic=True)
    n = min(len(a), len(b))
    # the first ~2 s: both start from silence, but not from exactly the same cache contents
    settle = min(2 * rate, n // 2)
    a, b = a[settle:n], b[settle:n]
    error_db = float(20 * np.log10(np.sqrt(np.mean((a - b) ** 2)) / (np.sqrt(np.mean(b ** 2)) + 1e-12) + 1e-12))
    real_time = chunk / rate
    print(f"{rate} Hz, chunk {chunk}: {per_call * 1000:.0f} ms per call for {real_time * 1000:.0f} ms of audio")
    print(f"without noise: output rms {np.sqrt(np.mean(b ** 2)):.4f}, difference {error_db:.1f} dB below the signal")

    a, _, _ = run_onnx(onnx.load(onnx_path), x, deterministic=False)
    print(f"with noise: output rms {np.sqrt(np.mean(a ** 2)):.4f}")
    ok = error_db < -60.0 and per_call < real_time
    print("OK" if ok else "CHECK FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

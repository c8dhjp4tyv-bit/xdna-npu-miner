#!/usr/bin/env python3
"""Build the one-column M3 BPP9000 recurrent K1 primitive."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np

import aie.iron as iron
from aie.iron import In, ObjectFifo, Out, Program, Runtime, Worker
from aie.iron.kernel import ExternalFunction
from aie.iron.device import from_name
from aie.utils.hostruntime import set_current_device


INPUT_DEVICE_BYTES = 2528
STATE_DEVICE_BYTES = 96
LUT_BYTES = 46 * 32
KERNEL_SOURCE = Path(__file__).with_name("k1_kernel.cc")


@iron.jit(aiecc_flags=["--alloc-scheme=basic-sequential"])
def xdna_k1(previous_state: In,
            next_state: Out):
    input_ty = np.ndarray[(INPUT_DEVICE_BYTES,), np.dtype[np.uint8]]
    state_ty = np.ndarray[(STATE_DEVICE_BYTES,), np.dtype[np.uint8]]

    kernel = ExternalFunction(
        "bpp9000_k1_tick",
        source_file=str(KERNEL_SOURCE),
        arg_types=[input_ty, state_ty],
    )

    input_in = ObjectFifo(input_ty, name="k1_input_in")
    state_out = ObjectFifo(state_ty, name="k1_state_out")

    def k1_worker(input_in, state_out, kernel):
        input_data = input_in.acquire(1)
        output = state_out.acquire(1)
        kernel(input_data, output)
        input_in.release(1)
        state_out.release(1)

    worker = Worker(
        k1_worker,
        [
            input_in.cons(),
            state_out.prod(),
            kernel,
        ],
    )

    runtime = Runtime()
    with runtime.sequence(input_ty, state_ty) as (input_buffer, output_buffer):
        runtime.start(worker)
        runtime.fill(input_in.prod(), input_buffer)
        runtime.drain(state_out.cons(), output_buffer, wait=True)

    return Program(iron.get_current_device(), runtime).resolve_program()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--xclbin-path", type=Path, required=True)
    parser.add_argument("--insts-path", type=Path, required=True)
    parser.add_argument("--dev", choices=("npu",), default="npu")
    args = parser.parse_args()

    args.xclbin_path.parent.mkdir(parents=True, exist_ok=True)
    args.insts_path.parent.mkdir(parents=True, exist_ok=True)
    set_current_device(from_name(args.dev, n_cols=1))
    xdna_k1.specialize().compile(
        xclbin_path=args.xclbin_path,
        inst_path=args.insts_path,
    )
    print(f"xclbin={args.xclbin_path}")
    print(f"insts={args.insts_path}")
    print("target=RyzenAI-npu1/aie2 columns=1")
    print("workload=BPP9000 K1 isolated recurrent LUT tick")
    print("combined_input_device_bytes=2528")
    print("logical_state_bytes=64 device_state_stride=96 state_offset=0")
    print("lut_rows=46 lut_logical_entries=27 lut_row_stride=32")
    print("neighbors_rows=64 neighbors_per_row=3 neighbor_offset=1568")
    print("updated_neurons=46 device_updated_words=48 updated_offset=2336")


if __name__ == "__main__":
    main()

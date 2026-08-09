#!/usr/bin/env python3
"""Build the one-column M4 repeated-tick/window BPP9000 artifact."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np

import aie.iron as iron
from aie.iron import In, ObjectFifo, Out, Program, Runtime, Worker
from aie.iron.kernel import ExternalFunction
from aie.iron.device import from_name
from aie.utils.hostruntime import set_current_device


INPUT_DEVICE_BYTES = 15488
OUTPUT_DEVICE_BYTES = 128
KERNEL_SOURCE = Path(__file__).with_name("m4_kernel.cc")


@iron.jit(aiecc_flags=["--alloc-scheme=basic-sequential"])
def xdna_m4(input_data: In, output_data: Out):
    input_ty = np.ndarray[(INPUT_DEVICE_BYTES,), np.dtype[np.uint8]]
    output_ty = np.ndarray[(OUTPUT_DEVICE_BYTES,), np.dtype[np.uint8]]

    kernel = ExternalFunction(
        "bpp9000_m4_dispatch",
        source_file=str(KERNEL_SOURCE),
        arg_types=[input_ty, output_ty],
    )

    input_fifo = ObjectFifo(input_ty, name="m4_input_in")
    output_fifo = ObjectFifo(output_ty, name="m4_output_out")

    def m4_worker(input_consumer, output_producer, kernel):
        input_tile = input_consumer.acquire(1)
        output_tile = output_producer.acquire(1)
        kernel(input_tile, output_tile)
        input_consumer.release(1)
        output_producer.release(1)

    worker = Worker(
        m4_worker,
        [input_fifo.cons(), output_fifo.prod(), kernel],
    )

    runtime = Runtime()
    with runtime.sequence(input_ty, output_ty) as (input_buffer, output_buffer):
        runtime.start(worker)
        runtime.fill(input_fifo.prod(), input_buffer)
        runtime.drain(output_fifo.cons(), output_buffer, wait=True)

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
    xdna_m4.specialize().compile(
        xclbin_path=args.xclbin_path,
        inst_path=args.insts_path,
    )
    print(f"xclbin={args.xclbin_path}")
    print(f"insts={args.insts_path}")
    print("target=RyzenAI-npu1/aie2 columns=1")
    print("workload=BPP9000 repeated K1/window score, one dispatch per operation")
    print(f"input_device_bytes={INPUT_DEVICE_BYTES} output_device_bytes={OUTPUT_DEVICE_BYTES}")
    print("state_reset=host-provided per operation; recurrent state is device-local within a dispatch")


if __name__ == "__main__":
    main()

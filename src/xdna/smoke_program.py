#!/usr/bin/env python3
"""Build the standalone M2 integer smoke program for XDNA1/AIE2."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np

import aie.iron as iron
from aie.iron import In, ObjectFifo, Out, Program, Runtime, Worker
from aie.iron.controlflow import range_
from aie.iron.device import from_name
from aie.utils.hostruntime import set_current_device


SMOKE_ELEMENT_COUNT = 32


@iron.jit(aiecc_flags=["--alloc-scheme=basic-sequential"])
def xdna_smoke(input_data: In, output_data: Out):
    data_type = np.ndarray[(SMOKE_ELEMENT_COUNT,), np.dtype[np.int32]]
    input_fifo = ObjectFifo(data_type, name="smoke_input")
    output_fifo = ObjectFifo(data_type, name="smoke_output")

    def smoke_core(input_consumer, output_producer):
        input_tile = input_consumer.acquire(1)
        output_tile = output_producer.acquire(1)
        for index in range_(SMOKE_ELEMENT_COUNT):
            output_tile[index] = input_tile[index] * 3 + 7
        input_consumer.release(1)
        output_producer.release(1)

    runtime = Runtime()
    with runtime.sequence(data_type, data_type) as (input_buffer, output_buffer):
        runtime.start(Worker(smoke_core, [input_fifo.cons(), output_fifo.prod()]))
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
    xdna_smoke.specialize().compile(
        xclbin_path=args.xclbin_path,
        inst_path=args.insts_path,
    )
    print(f"xclbin={args.xclbin_path}")
    print(f"insts={args.insts_path}")
    print("target=RyzenAI-npu1/aie2 columns=1")
    print("workload=out[i]=3*in[i]+7 elements=32 dtype=int32")


if __name__ == "__main__":
    main()

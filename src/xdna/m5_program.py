#!/usr/bin/env python3
"""Build a fixed-size M5 independent-window batch artifact."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np

import aie.iron as iron
from aie.iron import CompileTime, In, ObjectFifo, Out, Program, Runtime, Worker
from aie.iron.controlflow import range_
from aie.iron.device import Tile, from_name
from aie.iron.kernel import ExternalFunction
from aie.helpers.taplib import TensorAccessPattern
from aie.utils.hostruntime import set_current_device


INPUT_ITEM_BYTES = 15488
OUTPUT_ITEM_BYTES = 128
SUPPORTED_COLUMNS = (1, 2, 4)
SUPPORTED_BATCHES = (1, 2, 4, 8, 16)
KERNEL_SOURCE = Path(__file__).with_name("m5_kernel.cc")


@iron.jit(aiecc_flags=["--alloc-scheme=basic-sequential"])
def xdna_m5(
    input_data: In,
    output_data: Out,
    *,
    batch_size: CompileTime[int],
    columns: CompileTime[int],
):
    if columns not in SUPPORTED_COLUMNS:
        raise ValueError(f"columns must be one of {SUPPORTED_COLUMNS}")
    if batch_size not in SUPPORTED_BATCHES:
        raise ValueError(f"batch_size must be one of {SUPPORTED_BATCHES}")
    if batch_size % columns != 0:
        raise ValueError("batch_size must be divisible by columns")

    items_per_lane = batch_size // columns
    input_ty = np.ndarray[(batch_size * INPUT_ITEM_BYTES,), np.dtype[np.uint8]]
    output_ty = np.ndarray[(batch_size * OUTPUT_ITEM_BYTES,), np.dtype[np.uint8]]
    item_input_ty = np.ndarray[(INPUT_ITEM_BYTES,), np.dtype[np.uint8]]
    item_output_ty = np.ndarray[(OUTPUT_ITEM_BYTES,), np.dtype[np.uint8]]

    kernel = ExternalFunction(
        "bpp9000_m5_dispatch",
        source_file=str(KERNEL_SOURCE),
        arg_types=[item_input_ty, item_output_ty],
    )

    input_fifos = [
        ObjectFifo(item_input_ty, name=f"m5_input_lane_{lane}", depth=2)
        for lane in range(columns)
    ]
    output_fifos = [
        ObjectFifo(item_output_ty, name=f"m5_output_lane_{lane}", depth=2)
        for lane in range(columns)
    ]

    def m5_worker(input_consumer, output_producer, kernel):
        for _ in range_(items_per_lane):
            input_tile = input_consumer.acquire(1)
            output_tile = output_producer.acquire(1)
            kernel(input_tile, output_tile)
            input_consumer.release(1)
            output_producer.release(1)

    workers = [
        Worker(
            m5_worker,
            [input_fifos[lane].cons(), output_fifos[lane].prod(), kernel],
            tile=Tile(lane, 2),
        )
        for lane in range(columns)
    ]

    runtime = Runtime()
    with runtime.sequence(input_ty, output_ty) as (input_buffer, output_buffer):
        runtime.start(*workers)
        for lane in range(columns):
            input_offset = lane * items_per_lane * INPUT_ITEM_BYTES
            output_offset = lane * items_per_lane * OUTPUT_ITEM_BYTES
            input_tap = TensorAccessPattern(
                (1, batch_size * INPUT_ITEM_BYTES),
                input_offset,
                [1, 1, items_per_lane, INPUT_ITEM_BYTES],
                [0, 0, INPUT_ITEM_BYTES, 1],
            )
            output_tap = TensorAccessPattern(
                (1, batch_size * OUTPUT_ITEM_BYTES),
                output_offset,
                [1, 1, items_per_lane, OUTPUT_ITEM_BYTES],
                [0, 0, OUTPUT_ITEM_BYTES, 1],
            )
            runtime.fill(input_fifos[lane].prod(), input_buffer, input_tap)
            runtime.drain(output_fifos[lane].cons(), output_buffer, output_tap, wait=True)

    return Program(iron.get_current_device(), runtime).resolve_program()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--xclbin-path", type=Path, required=True)
    parser.add_argument("--insts-path", type=Path, required=True)
    parser.add_argument("--batch-size", type=int, choices=SUPPORTED_BATCHES, required=True)
    parser.add_argument("--columns", type=int, choices=SUPPORTED_COLUMNS, required=True)
    parser.add_argument("--dev", choices=("npu",), default="npu")
    args = parser.parse_args()
    if args.batch_size % args.columns != 0:
        parser.error("batch-size must be divisible by columns")

    args.xclbin_path.parent.mkdir(parents=True, exist_ok=True)
    args.insts_path.parent.mkdir(parents=True, exist_ok=True)
    set_current_device(from_name(args.dev, n_cols=args.columns))
    xdna_m5.specialize(batch_size=args.batch_size, columns=args.columns).compile(
        xclbin_path=args.xclbin_path,
        inst_path=args.insts_path,
    )
    print(f"xclbin={args.xclbin_path}")
    print(f"insts={args.insts_path}")
    print(f"target=RyzenAI-npu1/aie2 columns={args.columns}")
    print("workload=BPP9000 independent candidate/window pair")
    print(f"batch_size={args.batch_size}")
    print(f"items_per_lane={args.batch_size // args.columns}")
    print("lane_mapping=" + ",".join(f"lane{lane}->column{lane + 1}" for lane in range(args.columns)))
    print(f"input_item_bytes={INPUT_ITEM_BYTES} output_item_bytes={OUTPUT_ITEM_BYTES}")


if __name__ == "__main__":
    main()

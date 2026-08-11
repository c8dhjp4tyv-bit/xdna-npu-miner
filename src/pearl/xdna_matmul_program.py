#!/usr/bin/env python3
"""Compile the fixed Pearl P2 signed-int8 AIE2 GEMM artifact."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np

import aie.iron as iron
from aie.iron import In, ObjectFifo, Out, Program, Runtime, Worker
from aie.iron.device import Tile, from_name
from aie.iron.kernel import ExternalFunction
from aie.utils.hostruntime import set_current_device


P2_M = 4
P2_K = 64
P2_N = 8
P2_A_ELEMENTS = P2_M * P2_K
P2_B_ELEMENTS = P2_K * P2_N
P2_C_ELEMENTS = P2_M * P2_N
KERNEL_SOURCE = Path(__file__).with_name("xdna_matmul_kernel.cc")


@iron.jit(aiecc_flags=["--alloc-scheme=basic-sequential"])
def pearl_p2_gemm(left: In, right: In, output: Out):
    left_ty = np.ndarray[(P2_A_ELEMENTS,), np.dtype[np.int8]]
    right_ty = np.ndarray[(P2_B_ELEMENTS,), np.dtype[np.int8]]
    output_ty = np.ndarray[(P2_C_ELEMENTS,), np.dtype[np.int32]]
    left_tile_ty = np.ndarray[(P2_M, P2_K), np.dtype[np.int8]]
    right_tile_ty = np.ndarray[(P2_K, P2_N), np.dtype[np.int8]]
    output_tile_ty = np.ndarray[(P2_M, P2_N), np.dtype[np.int32]]

    # The AIE2 int8 MMUL does not consume a logical row-major tile as a
    # contiguous scalar stream.  These are the canonical IRON DMA transforms
    # for the AIE2 4x8x8 MAC shape: host buffers stay ordinary row-major
    # matrices, while the consumer-side views provide the lane order expected
    # by the project-owned kernel.  The inverse transform on C restores the
    # logical row-major output before the host drains it.
    left_fifo = ObjectFifo(left_tile_ty, name="pearl_p2_left")
    left_mem = left_fifo.cons().forward(
        name="pearl_p2_left_mem",
        dims_to_stream=[
            (P2_M // 4, 4 * P2_K),
            (P2_K // 8, 8),
            (4, P2_K),
            (8, 1),
        ],
    )
    right_fifo = ObjectFifo(right_tile_ty, name="pearl_p2_right")
    right_mem = right_fifo.cons().forward(
        name="pearl_p2_right_mem",
        dims_to_stream=[
            (P2_K // 8, 8 * P2_N),
            (P2_N // 8, 8),
            (8, P2_N),
            (8, 1),
        ],
    )
    output_fifo = ObjectFifo(output_tile_ty, name="pearl_p2_output")
    output_mem = output_fifo.cons().forward(
        name="pearl_p2_output_mem",
        dims_to_stream=[
            (P2_M // 4, 4 * P2_N),
            (4, 8),
            (P2_N // 8, 4 * 8),
            (8, 1),
        ],
    )
    kernel = ExternalFunction(
        "pearl_gemm_i8_i32",
        source_file=str(KERNEL_SOURCE),
        arg_types=[left_tile_ty, right_tile_ty, output_tile_ty],
    )

    def core_fn(left_consumer, right_consumer, output_producer, kernel):
        left_tile = left_consumer.acquire(1)
        right_tile = right_consumer.acquire(1)
        output_tile = output_producer.acquire(1)
        kernel(left_tile, right_tile, output_tile)
        left_consumer.release(1)
        right_consumer.release(1)
        output_producer.release(1)

    worker = Worker(
        core_fn,
        [left_mem.cons(), right_mem.cons(), output_fifo.prod(), kernel],
        tile=Tile(0, 2),
    )
    runtime = Runtime()
    with runtime.sequence(left_ty, right_ty, output_ty) as (left_buffer, right_buffer, output_buffer):
        runtime.start(worker)
        runtime.fill(left_fifo.prod(), left_buffer)
        runtime.fill(right_fifo.prod(), right_buffer)
        runtime.drain(output_mem.cons(), output_buffer, wait=True)
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
    pearl_p2_gemm.specialize().compile(
        xclbin_path=args.xclbin_path,
        inst_path=args.insts_path,
    )
    print(f"xclbin={args.xclbin_path}")
    print(f"insts={args.insts_path}")
    print("target=RyzenAI-npu1/aie2 columns=1")
    print("kernel=pearl_gemm_i8_i32")
    print("workload=A[4,64]xB[64,8]->C[4,8] int8*int8=int32")


if __name__ == "__main__":
    main()

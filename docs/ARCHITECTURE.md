# Architecture

This document is provisional until Milestone M0 validates the current Qubic mining workload and protocol from authoritative upstream sources.

## Design objective

Accelerate only compute-heavy, deterministic, batchable mining operations that fit XDNA1/AIE2 well, while keeping control-heavy and network-heavy work on the CPU.

## Proposed high-level pipeline

```text
                 +----------------------+
                 |  Qubic node / pool   |
                 +----------+-----------+
                            |
                            v
                 +----------------------+
                 | CPU network/job mgr  |
                 +----------+-----------+
                            |
                            v
                 +----------------------+
                 | CPU candidate/control|
                 +----------+-----------+
                            |
                   batched work buffers
                            |
                            v
           +-----------------------------------+
           | AMD XDNA1 / AIE2 compute kernels |
           | - deterministic scoring primitive |
           | - repeated vector/matrix work     |
           | - persistent/reused buffers       |
           +----------------+------------------+
                            |
                            v
                 +----------------------+
                 | CPU verify/reference |
                 +----------+-----------+
                            |
                            v
                 +----------------------+
                 | submit result/share  |
                 +----------------------+
```

## Module boundaries

### `src/core/`
Shared domain types, scheduling interfaces and orchestration primitives. Must not contain Qubic protocol assumptions that belong in `src/qubic/`.

### `src/qubic/`
Qubic-specific protocol, job representation, candidate semantics, score/result structures and submission logic. Exact contents are determined by M0.

### `src/cpu/`
Trusted CPU reference implementation. This is the correctness oracle for accelerated compute and should be structurally independent enough to avoid reproducing the same implementation bug as the NPU path.

### `src/xdna/`
Device discovery, XRT/MLIR-AIE/IRON integration, buffer lifecycle, dispatch, synchronization, capability/version reporting and explicit hardware failure behavior.

### `src/kernels/`
AIE2 kernel/graph sources. Kernels must expose deterministic inputs/outputs that can be compared with the CPU reference.

### `src/cli/`
Configuration, endpoint selection, identity/wallet/mining parameters, logging controls and user-facing runtime diagnostics.

## Host/NPU contract principles

- Batch enough independent work to amortize launch and transfer overhead.
- Prefer persistent/reused device buffers where correctness and runtime APIs permit.
- Avoid round trips between CPU and NPU inside inner loops unless unavoidable.
- Keep protocol/network objects out of kernel interfaces.
- Use explicit fixed-width datatypes across the boundary.
- Specify overflow, saturation, rounding, layout, alignment and endianness semantics.
- Never let an unavailable NPU silently use CPU while reporting NPU mode.

## Four-column strategy

XDNA1 target exposes a 4-column AIE2 array. M0/M3/M5 must determine whether the selected workload benefits from:

- independent batch partitioning by column;
- pipeline stages across columns;
- replicated kernels;
- shared/reused weight/state placement;
- another graph topology.

No four-column utilization claim is accepted without hardware evidence.

## Correctness architecture

For every accelerated primitive:

```text
input vector
    |--------------------------|
    v                          v
CPU golden                  XDNA1
    |                          |
    v                          v
expected output            NPU output
    \                          /
     +-------- compare -------+
               |
          pass / fail
```

Failures should preserve enough input/context to reproduce the discrepancy.

## Failure policy

The miner must distinguish at least:

- protocol/network error;
- invalid/stale work;
- CPU reference error;
- NPU device unavailable;
- NPU compile/load failure;
- NPU dispatch/runtime failure;
- CPU/NPU correctness mismatch;
- submission rejection;
- benchmark/telemetry unavailable.

Correctness mismatch is fatal for the affected accelerated path until explicitly recovered/revalidated.

## Open architecture questions for M0

1. What exactly is the current Qubic mining workload and scoring path?
2. Which operations dominate runtime?
3. Which datatypes and tensor/vector dimensions are current?
4. Which state changes per candidate and which data can remain resident on XDNA1?
5. What batch granularity is protocol-safe?
6. Which current upstream implementation is authoritative enough for interoperability checking?
7. What licensing constraints affect clean-room implementation?
8. What network/pool protocol/version must M6 support?

# Project Specification

## Project goal

Create a standalone miner that uses AMD XDNA1 NPU acceleration for compute-heavy portions of a cryptocurrency mining workload, with Qubic as the first target pending Milestone M0 verification of the current upstream algorithm and protocol.

The project is experimental systems engineering. Success requires both correctness and evidence of genuine NPU execution.

## Primary hardware target

- AMD Ryzen AI Hawk Point
- XDNA1 exposed as `RyzenAI-npu1`
- AIE2
- 4 columns
- Fedora Linux primary development environment

The project may later support other XDNA generations, but that is outside the initial scope unless a milestone explicitly adds it.

## Core design goals

1. **Correctness first** — establish a trusted CPU golden reference before acceleration.
2. **Real NPU execution** — accelerated paths must prove XDNA1 dispatch rather than silently falling back.
3. **Modularity** — separate protocol/network logic, CPU reference logic, accelerator runtime, kernels, and benchmarks.
4. **Agent continuity** — repository documents must allow a zero-context agent to resume safely.
5. **Reproducibility** — pin upstream versions and capture exact benchmark conditions.
6. **Measured performance** — report throughput, latency, errors, and energy/power when reliably measurable.
7. **License hygiene** — audit upstream code before reuse; use clean-room implementations when required.

## Initial architecture boundary

Provisional until M0 research is complete:

```text
Qubic node / pool
        |
        v
CPU network + job manager
        |
        v
CPU candidate generation / control
        |
        v
XDNA1 batched compute/scoring kernels
        |
        v
CPU golden/reference verification
        |
        v
share / solution submission
```

Only operations that materially benefit from XDNA1 should be placed on the NPU.

## Non-goals for the initial release

- covert or unauthorized mining;
- malware-like persistence or propagation;
- hiding resource consumption from the machine owner;
- claiming profitability before controlled measurements;
- supporting every coin or PoW algorithm;
- supporting XDNA2/other NPU generations before the XDNA1 path is correct and measured;
- optimizing before CPU/NPU correctness agreement exists.

## Evidence standard

A feature is not considered implemented merely because source code exists.

Examples:

- NPU support requires hardware execution evidence.
- protocol support requires interoperability evidence against a current upstream node/pool/reference implementation.
- performance claims require recorded benchmark methodology and results.
- milestone completion requires every acceptance criterion to pass.

## Planned source layout

```text
src/
  core/          shared types and orchestration
  qubic/         Qubic-specific protocol and workload logic
  cpu/           trusted CPU reference implementation
  xdna/          XDNA runtime, buffer and dispatch integration
  kernels/       AIE2 kernels / graph definitions
  cli/           user-facing CLI and configuration

tests/
  unit/
  integration/
  hardware/
  vectors/

benchmarks/
  cpu/
  xdna/
  comparative/

scripts/
  environment/setup/verification helpers
```

M0 may refine names and boundaries if authoritative research justifies changes.

## Initial success definition

A meaningful first release should be able to:

1. obtain or construct valid mining work from an authoritative Qubic-compatible source;
2. compute a trusted CPU result;
3. execute the selected heavy compute path on XDNA1;
4. show CPU/NPU correctness agreement;
5. submit valid work/share/result according to the current protocol;
6. run for an endurance interval without silent corruption or fallback;
7. publish controlled CPU-vs-NPU-vs-hybrid benchmark evidence.

# XDNA NPU Miner

Standalone AMD XDNA1 NPU-accelerated **Pearl (PRL)** mining research
project. Pearl is the active target; the completed Qubic implementation is
frozen reference-only and is not used as Pearl protocol evidence.

## Target platform

- AMD Ryzen AI Hawk Point
- XDNA1 / `RyzenAI-npu1`
- AIE2, 4-column array
- Fedora Linux as the primary development platform
- MLIR-AIE / IRON / XRT-based native NPU execution where appropriate

## Engineering principles

1. Correctness before performance.
2. Every NPU kernel must have a trusted CPU golden reference.
3. Never claim NPU execution without hardware evidence.
4. Never invent benchmark results.
5. Keep CPU control/network work separate from compute kernels that genuinely benefit from XDNA1.
6. The repository is the persistent memory for all AI coding agents.
7. Agents must read `AGENTS.md` and `docs/AI_HANDOFF.md` before making changes.
8. Milestones advance only after their acceptance criteria pass.

## Current status

P2 physical XDNA GEMM, P3 exact compute pipeline, P5 candidate/PlainProof,
P8 placement measurements, P9 benchmark, and the P11 operator CLI are
implemented and physically verified on Hawk Point. P4/P6 are locally tested;
the official useful-work provider, gateway/prover, and local/simnet node are
external blockers for live end-to-end acceptance. The current overall state is
`SOFTWARE_COMPLETE_E2E_BLOCKED`, not a profitability claim.

Quick start:

```bash
cargo build --manifest-path src/pearl/blake3_ffi/Cargo.toml --release --locked
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./scripts/build-pearl-xdna-gemm.sh build 4
./build/pearl-xdna-miner --self-test --artifact-dir build/pearl-xdna-gemm-p2-c4
./build/pearl-xdna-miner --benchmark --artifact-dir build/pearl-xdna-gemm-p2-c4
./build/pearl-xdna-miner --dry-run --fixture-work --artifact-dir build/pearl-xdna-gemm-p2-c4
```

Live operation requires an independently installed official gateway/prover,
current work tensors, and an explicit public mining address:

```bash
./build/pearl-xdna-miner --mine --mining-address PUBLIC_TAPROOT_ADDRESS \
  --artifact-dir build/pearl-xdna-gemm-p2-c4
```

See [`docs/pearl/OPERATIONS.md`](docs/pearl/OPERATIONS.md) for shutdown,
configuration, external boundaries, and troubleshooting.

## Repository memory

The project is intentionally designed for frequent handoffs between AI coding agents with limited context windows.

Start here:

1. `AGENTS.md`
2. `docs/AI_HANDOFF.md`
3. `docs/PROJECT_SPEC.md`
4. `docs/MILESTONES.md`
5. `docs/ARCHITECTURE.md`
6. `docs/DECISIONS.md`
7. `docs/TESTING.md`
8. `docs/UPSTREAM.md`

## Safety and scope

This repository is for legitimate cryptocurrency mining research on hardware the operator is authorized to use. It must not add hidden persistence, unauthorized resource use, credential theft, propagation, or stealth-mining behavior.

## License

Project license has not yet been finalized. M0 must audit upstream licenses before any reusable implementation is imported or adapted.

# Pearl (PRL) AI Handoff

## Current Pearl one-shot state (2026-08-11)

Current milestone: **P11 — final handoff**. Status: **COMPLETE**; overall
status is `SOFTWARE_COMPLETE_E2E_BLOCKED` because P7's official runtime is
unavailable. Branch: `feat/pearl-full-miner-one-shot`. Starting SHA:
`ba286d5770c93290a38784f89ae75cea87867b25`. Implementation checkpoint:
`9a83cfdb44762140afc0a147fe0ec6100391a767` (`pearl: complete protocol
boundaries and operator delivery`). Final evidence/handoff checkpoint:
`7a3d61ebbf5b7de0b21ae2ea23ec54638f78804a` (`pearl: finalize one-shot
evidence and handoff`).

Completed in this shot:

- P2 project-owned `4x64x8` signed-int8 AIE2 kernel, canonical IRON lane
  transforms, 100/100 exact physical cases, zero runtime failures/fallbacks.
- P3 exact noise → 256/64-bit checked GEMM → denoise → selected transcript →
  keyed BLAKE3 target path; 8 cases/64 dispatches passed.
- P4 strict JSON-RPC/job/header/target parser, deterministic fixture provider,
  and explicit unavailable official useful-work provider boundary.
- P5 256 physical dispatches feeding a CPU-verified 139,736-byte PlainProof
  with exact opening and serialization round-trip.
- P6 Unix/loopback TCP gateway transport with size/time/base64/target checks,
  typed errors, mock protocol tests, and optional BasicAuth-safe node adapter.
- P8 one/two/four-column artifacts and batch 1/2/4/8 physical sweep; c4/batch
  8 selected by measured dispatch throughput with zero mismatches.
- P9 actual runtime/firmware/driver/toolchain benchmark evidence; raw GEMM
  7.205k dispatches/s and full fixture candidate 3.01093/s, with null power
  and telemetry when unavailable.
- P11 safe `pearl-xdna-miner` CLI, config example, installation/operations
  documentation, explicit `--mine`, fixture dry-run, JSON status, and signal
  handling.

Files added include `src/pearl/{compute_pipeline,candidate,json,gateway,work,node,
miner_main}.*`, the AIE program/kernel, `scripts/build-pearl-xdna-gemm.sh`,
P2–P11 test/benchmark harnesses, `config.example.toml`, and milestone
evidence. Existing Qubic files and evidence remain frozen.

Tests executed: CMake Debug and Release configure/build PASS; full CTest
13/13 PASS; P2 100/100, P3 8/8, and P5 256-dispatch physical differential
tests PASS; gateway/work contract tests PASS; P8/P9 measurements PASS; CLI
help/version/hardware/self-test/benchmark/fixture dry-run PASS; P10 passed
1,800 seconds and 11,956,807 dispatches with zero mismatches/failures.
Aggregate and P11 evidence are in `docs/evidence/`.

Known external blockers: no official Pearl `pearld`, `pearl-gateway`,
useful-work/inference runtime, or local/simnet node is installed; therefore
P7 live gateway/prover/node acceptance is `BLOCKED`, not PASS. No public
mainnet payout address is configured. No pool/Stratum protocol is claimed.
The historical XDNA verification pin also reports `RUNTIME_VERSION_MISMATCH`
(expected amdxdna rc5, observed rc7); the observed rc7 stack is recorded in
the physical evidence and must not be silently relabeled.

Exact next task: install the pinned official Pearl node/gateway/prover/useful-
work runtime outside this repository and rerun the blocked P7 local/simnet
interoperability gate.

This is the authoritative handoff for the Pearl research track. Qubic remains
frozen/reference-only; do not resume Qubic M6/M7 or Qatum work while Pearl is
active.

## Historical P1 handoff (superseded by the one-shot state above)

The material below is retained as the original P1 checkpoint for auditability.
The authoritative active milestone, status, branch, and next task are in
`Current Pearl one-shot state` above.

**P1 — trusted CPU golden path**

**COMPLETE (historical checkpoint).** P1 completed at
`ba286d5770c93290a38784f89ae75cea87867b25`; the full-project branch now owns
the sequential P2-P11 implementation described above. Do not use this old
P1-only status to infer the current Pearl milestone.

## Branch and commit

- Branch: `feat/pearl-full-miner-one-shot`
- Starting `HEAD`: `ba286d5770c93290a38784f89ae75cea87867b25`
- Pinned Pearl source: `fe22b6a2b831d95b2f56564808f39d2f498f34a5`
- Implementation commit: `4d9132344f4f2a50c824164433a084f31ef135b7`
- Final evidence/handoff commit: `dde69536f7e2f1dfd906692f2e7750887b19a630`.

## Completed work

- Preserved the clean P0 checkpoint and created the dedicated P1 branch.
- Implemented independently designed `src/pearl/reference.hpp/.cpp` covering
  fixed-width header/config/pattern/public-data serialization, structural and
  current validation, fp32 quantization, checked signed products, deterministic
  noise, commitments, selected products, transcript tracing, jackpot targets,
  Merkle openings, and the pre-prover PlainProof envelope.
- Added a minimal Rust C ABI helper under `src/pearl/blake3_ffi/` pinned to
  official `blake3 = 1.8.2`; no Pearl hot-component source was copied,
  translated, or structurally reproduced.
- Resolved the range discrepancy: raw current mining matrices are `[-64,63]`,
  quantized output is `[-63,63]`; quantization uses fp32 scale, no zero point,
  ties-to-even rounding, and clamp. Arithmetic widens to int64 and rejects
  int32 overflow; it does not wrap or saturate.
- Added the fixed corpus under `tests/data/pearl/p1/`, including metadata,
  fixed digests, transcript values, Merkle values, PlainProof metadata, and
  negative categories.
- Added deterministic seeded randomized cases across ranks 32/64/128 and
  valid k/edge/seed/target combinations.

## Files changed by this track

- `CMakeLists.txt`
- `src/pearl/reference.hpp`
- `src/pearl/reference.cpp`
- `src/pearl/blake3_ffi.hpp`
- `src/pearl/blake3_ffi/Cargo.toml`
- `src/pearl/blake3_ffi/Cargo.lock`
- `src/pearl/blake3_ffi/src/lib.rs`
- `tests/pearl_cpu_tests.cpp`
- `tests/data/pearl/p1/README.md`
- `tests/data/pearl/p1/vectors.json`
- `docs/pearl/PROJECT_SPEC.md`
- `docs/pearl/ARCHITECTURE.md`
- `docs/pearl/MILESTONES.md`
- `docs/pearl/AI_HANDOFF.md`
- `docs/evidence/pearl-p1.json`
- relevant top-level testing/upstream/architecture/handoff records

## Tests executed and exact results

Before final commit, rerun and record the final results for:

```bash
cargo build --manifest-path src/pearl/blake3_ffi/Cargo.toml --release --locked
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 -m json.tool docs/evidence/pearl-p1.json >/dev/null
git diff --check
```

The focused target currently passes **67,765 assertions** in
`pearl_cpu_golden_tests`; the pinned upstream `pearl-blake3` black-box suite
passes **35/35** tests. The full CTest count and exact evidence/corpus digest
are recorded in this final handoff.

## Hardware tests actually executed

None for Pearl P1. No `/dev/accel` access, XDNA dispatch, NPU telemetry,
throughput, energy, hashrate, or profitability claim exists. Existing Qubic
XDNA evidence is not Pearl evidence and must not be relabeled.

## Known failures, limitations, and assumptions

- Pearl hot miner/proof/gateway component licenses remain unclear; clean-room
  implementation remains mandatory.
- The P1 PlainProof envelope stops immediately before CPU/Rust ZK generation;
  it is not a certificate and does not prove a block or share.
- No live node/pool protocol was needed or validated in P1. No official
  external pool transport was inferred.
- The BLAKE3 helper is a dependency boundary around the official crate, not a
  claim of compatibility with Pearl's local `pearl-blake3` license.
- The current pinned target comparison is `<=`, even though the test corpus
  separately exercises `<`, `==`, and `>` ordering outcomes.

## Architectural decisions

- Pearl remains isolated under `src/pearl/`; Qubic types and source are not
  reused.
- CPU owns serialization, configuration, commitments, noise derivation,
  opening verification, transcript verification, proof generation, and any
  future submission. Any future NPU result is untrusted until compared.
- P1 uses explicit fixed-width little-endian serialization instead of opaque
  host-layout or bincode dependence for the CPU/Rust boundary.
- Checked widened arithmetic is the canonical overflow behavior because P0
  found no pinned wrap/saturate rule.
- P1 has no benchmark claims and does not start P2 early.

## Things the next agent must not redo

- Do not re-audit the pinned P0 commit or clone/copy Pearl hot-component code.
- Do not replace the BLAKE3 dependency with Pearl-local BLAKE3 source.
- Do not weaken the fixed vectors, change `<=`, add CPU fallback claims, or
  treat Qubic hardware evidence as Pearl execution.
- Do not implement P2, live job parsing, wallet/network paths, or ZK proving in
  this milestone.

## Exact next task

After the final P1 commit is pushed and the remote branch is verified, the next
task is **P2: the minimal signed-int8 tile on physical XDNA1**, beginning with
batch one and exact comparison against this CPU oracle. It must add physical
device identity and dispatch evidence before making any Pearl NPU claim.

Relevant commands are the build/test commands above and:

```bash
./build/pearl_cpu_tests
./build/pearl_cpu_tests --dump
```

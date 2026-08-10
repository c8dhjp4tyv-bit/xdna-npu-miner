# Pearl (PRL) AI Handoff

This is the authoritative handoff for the Pearl research track. Qubic remains
frozen/reference-only; do not resume Qubic M6/M7 or Qatum work while Pearl is
active.

## Current milestone and status

**P1 — trusted clean-room CPU golden path and canonical vectors**

**COMPLETE pending final commit/evidence update.** P1 is CPU-only. No NPU,
node, pool, wallet, live mining, share/block submission, or ZK proof was run.

## Branch and commit

- Branch: `feat/pearl-p1-cpu-golden`
- Starting `HEAD`: `a15ed125295cc4361425a4b11159aa5744f3f160`
- Pinned Pearl source: `fe22b6a2b831d95b2f56564808f39d2f498f34a5`
- Final commit: pending until the verification gate is complete.

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
passes **35/35** tests. The final handoff must replace this paragraph with the
full CTest count and exact evidence/corpus digest after the last docs edit.

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

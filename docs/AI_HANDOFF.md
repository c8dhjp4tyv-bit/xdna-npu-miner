# AI Handoff

This file is the authoritative short-form state for the next zero-context
engineering agent.

## Current milestone

**M1 — CPU golden reference**

## Status

**COMPLETE**

M0 was externally reviewed and passed. M1 now provides the standalone scalar
CPU correctness oracle for pinned Qubic BPP9000 behavior. M2 is **NOT
STARTED**.

## Branch and commits

- Branch: `main`
- M0 completion commit:
  `057ee66c679a7ff89c1b90abefb72384184159e5`
- M1 implementation/checkpoint commit: record the exact SHA with
  `git rev-parse HEAD` after the final M1 checkpoint.

## M0 authority that M1 used

- Qubic core: `v1.301.3`,
  `a83f935406cd006b5b1a94971139e74d410ecb6d`.
- Qiner reference aid: `v1.302.3`,
  `11fb18a6f4944bb55fe103d3f263cb5d31e00200`.
- Canonical active algorithm: BPP9000 (`nonce[0] == 1`).
- Production shape: `N=18, M=1, T=8760, W=672, P=64, K=3, S=100`.
- Production score windows: `8088`; maximum ticks per window: `100000`.
- Timeout sentinel: `0xffffffff`; lower finite score is better.
- Runtime threshold is supplied by system information; the reference does not
  treat the Qiner example threshold `6469` as production truth.

Do not redo the M0 source/license audit unless a concrete upstream
contradiction affects the CPU semantics. Do not copy Qubic core, Qiner, or QLI
source. Their implementation source is Anti-Military licensed or unlicensed;
M1 is clean-room.

## Completed M1 work

- Added a standalone C++20/CMake library under `src/bpp9000/`.
- Added fixed-width public-key, mining-seed, nonce, task-header, topology,
  trit, LUT, recurrent-state, score, threshold, and mutation domain types.
- Added explicit little-endian 96-byte header serialization. The parser checks
  magic/version, dimensions, checked topology/data lengths, exact file length,
  role/index uniqueness and bounds, packed trit values `<243`, and digest
  metadata. Trailing bytes and truncation fail closed.
- Added canonical five-trit packing/unpacking; valid trits are exactly 0, 1,
  and 2, with 2 meaning UNKNOWN.
- Added dense logical LUT rows with 32-byte storage stride and a scalar,
  double-buffered recurrent tick. Every non-input update reads the previous
  state buffer and commits to the next buffer.
- Added one-window and full-window score paths, exact failure counting,
  timeout propagation, completion-aware score predicates, and runtime
  threshold predicates.
- Added canonical BPP9000 nonce checks (`nonce[0]==1`, `1<=nonce[1]<=10`,
  `nonce[2]==0`) and rejection of an all-zero mining seed.
- Added mutation selection, old/new value records, accept-if-`r <= current`,
  reverse-order rollback, and the default 100-step/101-score-call search.
- Added a seed-aware `CandidateRandomSource` boundary. Draw order and the
  64-byte random2-compatible padding sizes are explicit. The M1 fixture source
  is deterministic and non-cryptographic; no K12/random2 or signing code was
  copied or implemented.
- Added 100 generated small full-search cases and 10 independently generated
  production-shaped 44,744-byte cases. The production-shaped cases parse and
  execute one complete 672-sample window; they do not claim ten full
  production-score runs.
- Added `scripts/generate_corpus.sh` and a committed generator summary.

## Upstream cross-check result

The implementation was checked against the M0-derived facts from the pinned
core/Qiner revisions: exact header field order and sizes, base-3 packing,
topology role/index rules, three-neighbor LUT indexing, simultaneous
previous-state reads, signal-paced window scoring, timeout propagation,
canonical nonce fields, mutation selection/replacement, accept-if-`r <=
current`, and the 100-step/101-call lifecycle. Exact production K12/random2
outputs were not claimed because the required crypto provider is intentionally
an injection seam.

## Files changed in M1

- `CMakeLists.txt`
- `src/bpp9000/types.hpp`
- `src/bpp9000/task.hpp`
- `src/bpp9000/task.cpp`
- `src/bpp9000/random.hpp`
- `src/bpp9000/random.cpp`
- `src/bpp9000/reference.hpp`
- `src/bpp9000/reference.cpp`
- `tests/test_main.cpp`
- `scripts/generate_corpus.sh`
- `docs/TESTING.md`
- `docs/DECISIONS.md`
- `docs/ARCHITECTURE.md`
- `docs/PROJECT_SPEC.md`
- `docs/MILESTONES.md`
- `docs/AI_HANDOFF.md`

## Tests and exact results

Commands:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/bpp9000_tests
./scripts/generate_corpus.sh build
```

The direct test executable passes 8 groups and 361 assertions. The corpus
command reports:

```text
generator_version=m1-v1
generated_cases=100
production_shaped_cases=10
generated_digest=2979889feed3352b3c12831a301a357b6c9099f3de80b955f152c53bca2f8c03
production_digest=7c1da1028b9ecdbae54616654606185e62076ff7b69e209ecbf3d23f6a2fede1
```

The fixed test vector and corpus were executed twice with byte-identical
results. `git diff --check` passes.

## Hardware tests actually executed

None. No XDNA1 device, XRT runtime, AIE2 kernel, live Qubic node, network
adapter, or production K12 provider was exercised. M1 intentionally has no
hardware or networking dependency.

## Known limitations and unresolved behavior

1. The production task's topology/data hashes are KangarooTwelve-derived. M1
   has an explicit injected digest boundary and a test-only deterministic
   fingerprint, but no production K12 implementation. Select and license
   review that provider before production task loading or M6 integration.
2. The canonical production task bytes were not copied into this repository;
   M0 recorded their expected hashes. M1 production-shaped fixtures are
   independently generated and are not network truth.
3. Qatum/pool wire behavior remains unresolved and deferred. M1 does not
   depend on Qatum, QLI, or any pool.
4. No performance number, NPU activity, throughput, speedup, energy, or
   profitability claim exists.
5. Optional ASAN/UBSAN builds were attempted but the development image lacks
   the linker runtimes (`libasan.so.8.0.0` and `libubsan.so.1.0.0`). The normal
   warning-clean build and complete test suite pass.

## Architectural decisions to preserve

- CPU owns task validation, random/K12 orchestration, mutation control,
  accept/rollback, threshold/freshness policy, and canonical verification.
- Exact integer equality is mandatory; no tolerance, saturation, signed-trit
  reinterpretation, or silent CPU fallback is allowed.
- The value `2` is UNKNOWN, never `-1`.
- The first future XDNA mapping is independent candidate/window work across
  complete lanes; do not split a recurrent candidate across columns before
  measuring synchronization cost.
- Direct-node integration is the first future protocol path. Qatum is optional
  and must wait for a stable authoritative wire specification.

## Things the next agent MUST NOT redo

- Do not recreate the repository or repeat M0 research without a concrete
  contradiction.
- Do not copy Anti-Military-licensed Qubic source, Qiner source, QLI source,
  crypto code, or upstream task bytes.
- Do not replace the fixture random/digest seams with an unreviewed crypto
  implementation while calling M1 complete.
- Do not add AVX/SIMD, XDNA, XRT, MLIR-AIE, IRON, GPU, network, Qatum, pool,
  signing, or production mining-loop code in M1.
- Do not claim the production-shaped corpus is the canonical task or a
  production performance benchmark.
- Do not begin M3; M2 must establish runtime smoke evidence first.

## Exact next task: M2 runtime smoke

Start M2 only. Build a fail-closed, standalone XDNA1 runtime foundation with
device identity/capability reporting, project-owned dependency pins, buffer
allocation/synchronization, dispatch/completion/error handling, and a tiny
deterministic smoke program. It must identify `RyzenAI-npu1` when present,
prove actual dispatch, and classify missing hardware without a false NPU
result. The M2 runtime smoke input/output must be compared against the M1
oracle where applicable. Do not add BPP9000 kernels or mining networking.

## Resume commands

```bash
cd /home/umutcagand/xdna-npu-miner
git status -sb
git branch --show-current
git log --oneline -15
git rev-parse HEAD
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./scripts/generate_corpus.sh build
```

# AI Handoff

This file is the authoritative short-form state for the next zero-context
engineering agent.

## Current milestone

**M3 — first XDNA1 BPP9000 K1 recurrent-tick kernel**

## Status

**COMPLETE** — the physical K1 differential acceptance run, M1/M2 regressions,
negative paths, documentation, and final repository checks pass on the current
host.

M0, M1, and M2 were externally reviewed and passed. M4 is **NOT STARTED**.

## Branch and commits

- Branch: `main`
- M0 completion commit:
  `057ee66c679a7ff89c1b90abefb72384184159e5`
- M1 implementation/checkpoint commit:
  `323e2bcdc6885ceb4c6ec3ce65af7e651b3e85bb`
- M1 handoff commit:
  `749311c16bf40604aab7521625a58f859e6a9d75`
- M2 completion commit:
  `4ae226a048a65fed67fd7b8ab6a8feee9ec4c696`
- M3 implementation commit: recorded after the focused checkpoint is created.

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

## Completed M2 work

- Added `src/xdna/` with typed errors, capability discovery, an explicit smoke
  buffer contract, XRT device/hardware-context setup, instruction loading,
  persistent buffers, dispatch/wait handling, and CPU-oracle evidence.
- Added `xdna_probe`, `xdna_smoke`, and pure `xdna_contract_tests` targets. The
  capability surface includes `SUPPORTED_XDNA1`, `NO_XDNA_DEVICE`,
  `WRONG_XDNA_GENERATION`, `XRT_UNAVAILABLE`, `DRIVER_UNAVAILABLE`,
  `FIRMWARE_UNAVAILABLE_OR_UNKNOWN`, `TOOLCHAIN_UNAVAILABLE`,
  `RUNTIME_VERSION_MISMATCH`, and `DEVICE_OPEN_FAILED`.
- Added the standalone Iron/MLIR-AIE smoke program and reproducible artifact
  builder. The device program contains no BPP9000 operation.
- Added project-owned `runtime-pins.json`, capability/artifact/smoke scripts,
  and `docs/evidence/m2-xdna-smoke.json`.
- The host path explicitly allocates instruction/input/output BOs, performs
  H2D and D2H synchronization, waits for `ERT_CMD_STATE_COMPLETED`, and has
  no CPU fallback. Context-creation failures have a distinct typed error.

The verified current host is Fedora Linux 45 prerelease on AMD Ryzen 7 250
with Radeon 780M. It reports `RyzenAI-npu1`, `aie2`, BDF `0000:06:00.1`,
`/dev/accel/accel0`, XRT topology `6x5`, and four available columns. The
recorded runtime pins are amdxdna/kernel
`7.2.0-0.rc5.260731.8ba098e6.443.vanilla.fc45.x86_64`, firmware `1.5.5.391`,
XRT `2.26.0` hash
`8bf2fc4c090540dcf7872243ab67779ae74ef5e3`, MLIR-AIE commit
`57d7494e99c214f5f53b328a0ed43a99e759e835`, `mlir_aie` `1.3.4`, CPython
`3.12.13`, and Peano `llvm-aie 21.0.0.2026072001+ce8c0f8f`. The artifact
uses one column, kernel `MLIR_AIE`, UUID
`2a4f5f1f-3f1e-33ce-3f3f-56d4cf90be92`, and workload
`int32[32] out[i] = 3 * in[i] + 7`.

The 100-dispatch acceptance run completed 100 XRT dispatches and 100 exact
CPU matches with zero output mismatches and zero runtime failures. It recorded
200 explicit H2D and 100 explicit D2H synchronizations. The related
`hawkpoint-npu-llm` checkout was reference-only; its old `...441...` kernel
pin differs from the current host's `...443...` stack, which was not changed.

## Completed M3 work

- Extended the M1 reference with `RecurrentState::load_current` and the public
  `recurrent_tick` oracle. It retains the scalar double-buffered semantics:
  input roles are held, updated rows read the previous state, and the next
  state is committed simultaneously.
- Added the typed K1 host contract under `src/xdna/k1.hpp` and `k1.cpp`:
  exact logical lengths, trit/topology validation, 32-byte LUT stride,
  deterministic pack/unpack, padding isolation, and exact mismatch indices.
- Added the clean-room AIE2 kernel in `src/xdna/k1_kernel.cc`. It performs
  only one physical recurrent LUT tick and never computes a CPU expected
  result or silently falls back to CPU.
- Added the one-column Iron/MLIR-AIE artifact generator and
  `scripts/build-xdna-k1.sh`. The device input is one 2,528-byte aligned arena:
  state at offset 0, LUT at 96, neighbors at 1,568, and updated rows at 2,336;
  the output BO is 96 bytes with a 64-byte logical prefix. This layout was
  selected after the compiler rejected the independent-stream shape because of
  the one-column DMA channel budget.
- Added `xdna_k1_differential`, deterministic edge/fixed/random vectors,
  mismatch JSON capture, and `scripts/run-m3-validation.sh`.

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

M2 also changed or added:

- `CMakeLists.txt`, `runtime-pins.json`, and `tests/xdna_contract_tests.cpp`;
- `src/xdna/` runtime, smoke host, and Iron artifact sources;
- `scripts/verify-xdna1.sh`, `scripts/build-xdna-smoke.sh`, and
  `scripts/run-xdna-smoke.sh`;
- `docs/evidence/m2-xdna-smoke.json` and the M2 updates to
  `docs/TESTING.md`, `docs/ARCHITECTURE.md`, `docs/DECISIONS.md`,
  `docs/BENCHMARKS.md`, and `docs/UPSTREAM.md`.

M3 also changed or added:

- `src/bpp9000/reference.hpp` and `src/bpp9000/reference.cpp` for the K1
  oracle boundary;
- `src/xdna/k1.hpp`, `src/xdna/k1.cpp`, `src/xdna/k1_kernel.cc`,
  `src/xdna/k1_program.py`, and the K1 runtime additions;
- `tests/k1_vectors.hpp`, `tests/k1_vectors.cpp`,
  `tests/k1_contract_tests.cpp`, and `tests/k1_differential.cpp`;
- `scripts/build-xdna-k1.sh`, `scripts/run-m3-validation.sh`, and
  `docs/evidence/m3-k1-differential.json`;
- the M3 updates to `docs/AI_HANDOFF.md`, `docs/MILESTONES.md`,
  `docs/ARCHITECTURE.md`, `docs/TESTING.md`, `docs/DECISIONS.md`, and
  `docs/BENCHMARKS.md`.

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

M2 commands and results:

```bash
./scripts/verify-xdna1.sh
./scripts/build-xdna-smoke.sh
./scripts/run-xdna-smoke.sh --iterations 1
./scripts/run-xdna-smoke.sh --iterations 100
python3 -m json.tool docs/evidence/m2-xdna-smoke.json
```

The capability probe and artifact build pass. The one-dispatch smoke and the
100-dispatch run both report `NPU SMOKE PASS`; the evidence JSON validates.
The M2 completion SHA is recorded above.

M3 commands and exact results:

```bash
./scripts/run-m3-validation.sh
python3 -m json.tool docs/evidence/m3-k1-differential.json
git diff --check
```

The final differential run exercised 37 edge cases, 100 fixed cases, and
1,000 seeded random cases with generator `m3-k1-v1` and seed
`5562880460839399681`. It completed 1,139 physical K1 dispatches with 1,139
successful dispatches and exact logical matches, zero mismatches, zero runtime
failures, 2,278 H2D synchronizations, and 1,139 D2H synchronizations. The
machine-readable record is `docs/evidence/m3-k1-differential.json`.

## Hardware tests actually executed

The physical XDNA1/AIE2 path was exercised on the current host. `xrt-smi`
reported `RyzenAI-npu1`, firmware `1.5.5.391`, XRT `2.26.0`, and the current
amdxdna/kernel string recorded in `runtime-pins.json`. The generated K1
artifact was loaded into an XRT hardware context and dispatched 1,139 times
with exact output comparison against M1. The final artifact used one AIE2
column, kernel `MLIR_AIE`, xclbin UUID
`8e1b4ae5-7811-4641-fa48-99bfeb489071`, and the hashes recorded in the
evidence JSON. No live Qubic node, network adapter, production K12 provider,
state-resident multi-tick workload, or four-column workload was exercised.

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
   profitability claim exists. M3 records physical dispatch counters only;
   it is not a timing or throughput benchmark.
5. Optional ASAN/UBSAN builds were attempted but the development image lacks
   the linker runtimes (`libasan.so.8.0.0` and `libubsan.so.1.0.0`). The normal
   warning-clean build and complete test suite pass.
6. A second-generation device is not present, so a physical
   `WRONG_XDNA_GENERATION` run was not available. Forced device-execution,
   context-creation, and output-mismatch failures were not manufactured on the
   healthy device; their typed fail-closed paths are implemented.
7. M3 proves one isolated tick per host dispatch. State is not retained on the
   device across ticks, and no full-window score, mutation loop, batching, or
   four-column mapping is implemented.

## Architectural decisions to preserve

- CPU owns task validation, random/K12 orchestration, mutation control,
  accept/rollback, threshold/freshness policy, and canonical verification.
- Exact integer equality is mandatory; no tolerance, saturation, signed-trit
  reinterpretation, or silent CPU fallback is allowed.
- The value `2` is UNKNOWN, never `-1`.
- The first future XDNA mapping is independent candidate/window work across
  complete lanes; do not split a recurrent candidate across columns before
  measuring synchronization cost.
- M2's smoke artifact is one-column `int32[32]` arithmetic only. It proves the
  runtime boundary and is not a BPP9000 kernel, mining benchmark, or four-column
  utilization result.
- M3's K1 artifact is one-column isolated recurrent compute only. Its exact
  logical contract and combined device arena are recorded in the M3 evidence;
  do not treat its dispatch count as a performance result.
- Direct-node integration is the first future protocol path. Qatum is optional
  and must wait for a stable authoritative wire specification.

## Things the next agent MUST NOT redo

- Do not recreate the repository or repeat M0 research without a concrete
  contradiction.
- Do not copy Anti-Military-licensed Qubic source, Qiner source, QLI source,
  crypto code, or upstream task bytes.
- Do not replace the fixture random/digest seams with an unreviewed crypto
  implementation while calling M1 complete.
- Do not add AVX/SIMD, GPU, network, Qatum, pool, signing, or production
  mining-loop code while completing M3.
- Do not claim the production-shaped corpus is the canonical task or a
  production performance benchmark.
- Do not claim four-column execution, throughput, speedup, power, or
  profitability from the M2 smoke or M3 K1 dispatch counts.
- Do not redo the M3 K1 differential run unless source, toolchain, artifact, or
  contract changes require it.

## Exact next task: M4 full CPU/NPU correctness path

M4 may compose the verified K1 primitive into a full-window or justified fused
score path while retaining the M1 scalar oracle as the submission authority.
It must add state reset/feed/settling, timeout and score reduction semantics,
multiple candidates/batches, and saved exact mismatch artifacts. Do not begin
network integration, pool work, four-column tuning, or performance optimization
as part of M4.

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
./scripts/run-m3-validation.sh
```

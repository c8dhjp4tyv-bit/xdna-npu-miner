# AI Handoff

This file is the authoritative short-form state for the next zero-context
engineering agent.

## Current milestone

**M6 — Qubic direct-node integration**

## Status

**IN PROGRESS** — this is a crash-recovery continuation. The repository was
recovered before M6 implementation: no M6 source, tests, commits, or evidence
file survived in the worktree. M0 through M5 remain the inherited baseline;
their validations must be rerun after recovery before M6 can be considered
complete.

## Crash recovery checkpoint

- Recovery date: 2026-08-09.
- Branch: `main`.
- HEAD: `62a84d5d8674a1a74c1b7348e1fa41c85348e026` (`record M5 handoff
  commit`), also `origin/main`.
- Working tree: four modified, unstaged evidence JSON files:
  `docs/evidence/m2-xdna-smoke.json`,
  `docs/evidence/m3-k1-differential.json`,
  `docs/evidence/m4-full-score-differential.json`, and
  `docs/evidence/m5-batching-four-column.json`.
- Staged files: none.
- Recovered commits: the M0–M5 commit chain through `62a84d5`; no separate M6
  commit was found in `git log --all` or the reflog.
- `git fsck --full`: five dangling objects (two trees and three blobs) only;
  no missing or corrupt object was reported. No conflict markers, editor
  temporary files, or M6 files were found.
- The four evidence diffs are regenerated artifact UUID/hash/timing records;
  they are retained for inspection and are not treated as committed evidence
  until their producing validation commands are rerun and the records are
  validated.
- First incomplete M6 task: all M6 components, beginning with current direct-
  node protocol revalidation and the clean-room framing/work-context boundary.

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
- M3 implementation commit:
  `f5836e2fb0fd57d03babe6c3c3647db06fd0c269`
- M4 implementation/evidence commit: `c6308b1`
- M5 implementation/evidence commit: `bd9a349`
- M6 recovery checkpoint: uncommitted in the current working tree; no M6
  commit existed before this session.

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

## Completed M4 work

- Added the public `CandidateMaterial` seam in the M1 reference so CPU-owned
  candidate materialization is shared exactly by the M4 verifier without
  moving mutation or random-source authority to the device.
- Added `src/xdna/m4.hpp/.cpp` with the fixed 15,488-byte input/128-byte output
  contract, M3-compatible 64-byte state/46x32 LUT/64x3 topology/18 input-role
  schema, trit and role validation, explicit timeout sentinel transport, and
  exact packed-output validation.
- Added `src/xdna/m4_score.hpp/.cpp` and `verification.*`. Raw NPU results are
  separate from the verified result; every window is independently recomputed
  by M1, compared field-for-field, and rejected on any mismatch. CPU retains
  state reset, window reduction, random materialization, mutation,
  accept/rollback, and candidate authority.
- Added the clean-room one-column AIE2 artifact in `m4_kernel.cc` and
  `m4_program.py`, plus `scripts/build-xdna-m4.sh`. It supports isolated K1,
  repeated-tick, and one-window signal-paced score modes with no CPU fallback.
- Added `m4_contract_tests`, deterministic fixed/random vectors, physical
  `xdna_m4_differential`, mismatch JSON diagnostics, and
  `scripts/run-m4-validation.sh`. The final driver keeps M2/M3 evidence in
  temporary files and leaves the committed M2/M3 records unchanged.
- Added `docs/evidence/m4-full-score-differential.json` with final physical
  device, toolchain, artifact, dispatch, differential, verification, and
  negative-path evidence.

## Completed M5 work

- Added the fixed-width M5 contract under `src/xdna/m5.hpp` and `m5.cpp`.
  One item is one complete independent M4 `WindowScore` operation with
  candidate index, window index, explicit state/LUT/topology/input/target
  offsets, 15,488-byte input stride, 128-byte output stride, result ordering,
  status, and per-item error fields. Contract validation rejects malformed
  shapes, trits, roles, topology, stale/sentinel output, and wrong result
  magic.
- Added `src/xdna/m5_kernel.cc` and `m5_program.py`. The clean-room device
  kernel runs the M4 window semantics only; CPU mutation, accept/rollback,
  reduction, and canonical verification remain host-owned. Fixed artifact
  variants exist for batch sizes 1, 2, 4, 8, and 16 with one, two, or four
  columns where divisible.
- Added explicit lane workers and corrected the runtime DMA tap to transfer
  every `items_per_lane` record. Generated `npu1_1col`, `npu1_2col`, and
  `npu1_4col`/partition metadata are retained under the build artifact paths
  and summarized in `docs/evidence/m5-batching-four-column.json`.
- Extended `XdnaRuntime` with M5 BO allocation/reuse, full input/output
  rewrites per dispatch, explicit H2D/D2H counters, dispatch-wait timing, and
  fail-closed per-item output validation. M4 and M5 measurements use separate
  hardware-context lifetimes because this host rejects concurrent contexts.
- Added `m5_contract_tests`, `xdna_m5_differential`,
  `scripts/build-xdna-m5.sh`, `scripts/aggregate-m5-evidence.py`, and
  `scripts/run-m5-validation.sh`. The runner exercises ordered, reversed,
  `A,A,B,A`, unique-lane, repeated-BO, mutation-visible, rollback, timeout,
  and finite-score cases against the M1 CPU oracle.

## Completed M6 work in this recovery checkpoint

- Revalidated the current public Qubic core/Qiner refs with `git ls-remote`;
  the pinned core `a83f935406cd006b5b1a94971139e74d410ecb6d` and Qiner
  `11fb18a6f4944bb55fe103d3f263cb5d31e00200` still match `main` and their
  recorded tags. No upstream source was copied.
- Added `src/qubic/direct_node.hpp/.cpp` with strict 8-byte frame parsing,
  incremental/partial-read handling, exact 128-byte `RespondSystemInfo`
  parsing, local task identity, BPP9000 algorithm selection, WorkContext
  freshness, CPU/NPU exact score/threshold/nonce gates, deterministic direct
  solution serialization, bounded TCP connect/read/write behavior, bounded
  reconnect, secret-safe environment configuration, and explicit live-send
  opt-in.
- Added an injected `CryptoProvider` boundary and fail-closed
  `UnavailableCryptoProvider`. No production K12/FourQ-compatible provider or
  secret was selected; the test provider is deterministic and
  non-cryptographic only.
- Added `qubic_direct_node_tests` covering fragmented frames, system-info
  fields, stale context/seed, unsupported algorithm, task mismatch,
  CPU/NPU mismatch, invalid nonce, timeout, threshold rejection, malformed
  context, deterministic solution bytes, bounded reconnect, mock request and
  mock submission, disabled live submission, and secret redaction.
- Added `docs/evidence/m6-direct-node.json` with the recovery, protocol,
  offline/mock, no-send, live-gate, and regression state. M6 remains
  **IN PROGRESS** because live interoperability and production crypto are not
  exercised.

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

M4 also changed or added:

- `src/bpp9000/reference.hpp` and `src/bpp9000/reference.cpp` for the shared
  candidate-material seam;
- `src/xdna/m4.hpp`, `src/xdna/m4.cpp`, `src/xdna/m4_score.hpp`,
  `src/xdna/m4_score.cpp`, `src/xdna/verification.hpp`,
  `src/xdna/verification.cpp`, and the M4 additions to `runtime.*`;
- `src/xdna/m4_kernel.cc`, `src/xdna/m4_program.py`,
  `scripts/build-xdna-m4.sh`, and `scripts/run-m4-validation.sh`;
- `tests/m4_vectors.*`, `tests/m4_contract_tests.cpp`, and
  `tests/m4_differential.cpp`;
- `docs/evidence/m4-full-score-differential.json`, plus the M4 updates to
  `docs/MILESTONES.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md`,
  `docs/DECISIONS.md`, `docs/BENCHMARKS.md`, and this handoff.

M5 also changed or added:

- `src/xdna/m5.hpp`, `src/xdna/m5.cpp`, `src/xdna/m5_kernel.cc`,
  `src/xdna/m5_program.py`, and the M5 additions to `src/xdna/runtime.*`;
- `tests/m5_contract_tests.cpp`, `tests/m5_differential.cpp`, and the M5
  CMake/CTest targets;
- `scripts/build-xdna-m5.sh`, `scripts/aggregate-m5-evidence.py`, and
  `scripts/run-m5-validation.sh`;
- `docs/evidence/m5-batching-four-column.json`, plus the M5 updates to
  `docs/MILESTONES.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md`,
  `docs/DECISIONS.md`, `docs/BENCHMARKS.md`, and this handoff.

M6 recovery checkpoint also changed or added:

- `src/qubic/direct_node.hpp`, `src/qubic/direct_node.cpp`, and
  `tests/qubic_direct_node_tests.cpp`;
- the `qubic_direct_node` library and CTest target in `CMakeLists.txt`;
- `docs/evidence/m6-direct-node.json`;
- the M6 updates to `docs/UPSTREAM.md`, `docs/PROJECT_SPEC.md`,
  `docs/MILESTONES.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md`,
  `docs/DECISIONS.md`, and this handoff.

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

M4 commands and exact results:

```bash
./scripts/run-m4-validation.sh
python3 -m json.tool docs/evidence/m4-full-score-differential.json
git diff --check
```

The final physical run used the pinned random seed
`5562880460839399681` and completed:

```text
repeated_tick_cases=1000
one_window_cases=100
multi_window_cases=1000
fixed_cases=100
random_cases=1000
full_score_cases=11
production_shaped_cases=1
candidate_cases=2
physical_dispatches=13460
successful_dispatches=13460
exact_score_runs=213
exact_comparisons=4313
candidate_score_calls=202
candidate_window_comparisons=1212
score_mismatches=0
runtime_failures=0
explicit_h2d_syncs=26920
explicit_d2h_syncs=13460
```

The differential evidence records 12,460 compared windows, 34 timeout
matches, 3,279 finite score matches, and zero mismatches. The 11 full-score
runs include ten generated small cases and one independent production-shaped
`T=8760/W=672` case with all 8,088 windows. Each of the two candidate paths
completed 101 score calls and matched the standalone CPU candidate's final
current/best LUT and score state. The M4 contract tests also cover malformed
trits/topology/sequences, invalid window bounds, timeout serialization, and
score mismatch rejection.

M5 commands and exact results:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./scripts/run-m5-validation.sh
python3 -m json.tool docs/evidence/m5-batching-four-column.json
git diff --check
```

`run-m5-validation.sh` reruns M1 corpus/digest checks, M2 smoke, M3
differential, and M4 full-score validation before building and running the
M5 matrix. It physically accepted these `(batch_size, columns)` variants:
`(1,1)`, `(2,1)`, `(4,1)`, `(2,2)`, `(4,2)`, `(8,2)`, `(4,4)`, `(8,4)`,
and `(16,4)`. Each configuration used 16 logical items, two warm-ups, five
measured repeats, and 80/80 exact measured item matches with zero mismatches
and zero runtime failures. The runner also recorded exact ordered/reversed/
`A,A,B,A` isolation and mutation/rollback visibility matches.

The M4 identical-work baseline was 80 physical dispatches, 160 H2D syncs,
80 D2H syncs, 1,246,800 H2D bytes, 10,240 D2H bytes, and median/p95 wall
time 2.479492/3.015180 ms. The best raw M5 record was batch 16/four columns:
five dispatches, 10 H2D syncs, five D2H syncs, 1,249,280 H2D bytes, 10,240
D2H bytes, and median/p95 wall time 1.067016/1.451890 ms. M5 H2D bytes are
slightly larger because its fixed input stride includes 31 explicit padding
bytes per item. The full raw timing samples, artifact SHA-256 values,
instruction hashes, UUIDs, generated placement, and buffer footprints are in
`docs/evidence/m5-batching-four-column.json`; no speedup, hashrate, power,
energy, profitability, or network claim is made.

M6 offline/mock commands and exact results:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 -m json.tool docs/evidence/m6-direct-node.json
git diff --check
```

After recovery and the M6 additions, the build completed with no compiler
errors and CTest reported `6/6` tests passed. The new direct-node test uses
one-byte response reads, bounded three-attempt reconnect with two injected
failures, exact mock system-info request parsing, deterministic test-only
solution serialization, and zero sends for every invalid-case gate. It does
not claim K12/FourQ correctness or live node interoperability.

## Hardware tests actually executed

The physical XDNA1/AIE2 path was exercised on the current host. `xrt-smi`
reported `RyzenAI-npu1`, firmware `1.5.5.391`, XRT `2.26.0`, and the current
amdxdna/kernel string recorded in `runtime-pins.json`. The generated K1
artifact was loaded into an XRT hardware context and dispatched 1,139 times
with exact output comparison against M1. The final artifact used one AIE2
column, kernel `MLIR_AIE`, xclbin UUID
`8e1b4ae5-7811-4641-fa48-99bfeb489071`, and the hashes recorded in the
evidence JSON.

The final M4 artifact was also loaded into an XRT hardware context on the same
device and dispatched 13,460 times. It used one AIE2 column, kernel
`MLIR_AIE`, xclbin SHA-256
`618d1fc500ab07b22e48854dd75409746c11f7f31864d3c9e989441bd0163ec2`,
instruction SHA-256
`cc811e1751208451a5979e117c91dc238809403602c58b098d1acd55edc3a5d6`, and
runtime UUID `409da7fb-4047-d01e-7614-0e73947ad2bc`. M4 state was reset by the
host per window and held device-local only within a dispatch.

M5 physically loaded and ran all nine accepted fixed artifacts. The selected
batch-16/four-column artifact used xclbin SHA-256
`cc64f6719376fbbda50d094ec6980f0bc4590a40b4e60f83dfd567a677987724`,
instruction SHA-256
`8d4aae92b9edde7b9e2c6725ba74b3dbd91bc5d1e2e800ab1c4b1667f9a861f5`, and
runtime UUID `3ce448ce-c7e2-2667-7606-8843f2567110`. Its generated partition
metadata reports width 4, start column 0, four row-2 workers, and lane item
ranges 0–3, 4–7, 8–11, and 12–15. Every lane processed distinct fixture
inputs and returned an exact CPU-verified result. The M4 baseline and M5
contexts were created in separate lifetimes because concurrent context setup
returns `DRM_IOCTL_AMDXDNA_CREATE_HWCTX` `err=-19` on this host.

No live Qubic node or production K12/signing provider was exercised. M6
protocol behavior was exercised only through the offline/mock boundary recorded
in `docs/evidence/m6-direct-node.json`.

## Known limitations and unresolved behavior

1. The production task's topology/data hashes are KangarooTwelve-derived. M1
   has an explicit injected digest boundary and a test-only deterministic
   fingerprint, but no production K12 implementation. Select and license
   review that provider before production task loading or live M6 submission.
2. The canonical production task bytes were not copied into this repository;
   M0 recorded their expected hashes. M1 production-shaped fixtures are
   independently generated and are not network truth.
3. Qatum/pool wire behavior remains unresolved and deferred. M1 does not
   depend on Qatum, QLI, or any pool.
4. M5 records raw timing, transfer, and dispatch measurements for an identical
   16-item comparison workload. They are not converted into a speedup,
   hashrate, energy, or profitability claim; M3/M4 records remain correctness
   evidence only.
5. Optional ASAN/UBSAN builds were attempted but the development image lacks
   the linker runtimes (`libasan.so.8.0.0` and `libubsan.so.1.0.0`). The normal
   warning-clean build and complete test suite pass.
6. A second-generation device is not present, so a physical
   `WRONG_XDNA_GENERATION` run was not available. Forced device-execution,
   context-creation, and output-mismatch failures were not manufactured on the
   healthy device; their typed fail-closed paths are implemented.
7. M5 does not retain a device-resident task/LUT/context across logical
   mutations or task changes. It safely reuses XRT BO allocations by rewriting
   the full input/output arenas every dispatch. The selected batch-16/four-
   column configuration is a local compute backend only. M6 wraps it with an
   offline-tested direct-node boundary, but production crypto and live
   interoperability remain unavailable.

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
- M4's artifact is one-column repeated-tick/one-window scoring. The host
  resets each window to UNKNOWN, transfers the full operation arena, compares
  every result to the M1 scalar oracle, and performs full-score reduction;
  `persistent_buffers=false` is intentional. CPU remains the only candidate
  and submission authority.
- M5's selected backend is a fixed batch-16/four-column artifact for complete
  independent window operations. Input stride is 15,488 bytes, output stride
  is 128 bytes, lane order is contiguous and stable, BO allocations are
  persistent but full arenas are rewritten/sentinel-initialized per dispatch,
  and CPU recomputation remains mandatory. Do not silently change the work
  unit to candidate mutation/search or split one recurrent window across
  columns.
- Direct-node integration is the current M6 protocol path. Qatum is optional
  and must wait for a stable authoritative wire specification; M7's continuous
  supervisor remains out of scope.

## Things the next agent MUST NOT redo

- Do not recreate the repository or repeat M0 research without a concrete
  contradiction.
- Do not copy Anti-Military-licensed Qubic source, Qiner source, QLI source,
  crypto code, or upstream task bytes.
- Do not replace the fixture random/digest seams with an unreviewed crypto
  implementation while calling M1 complete.
- Do not add AVX/SIMD, GPU, Qatum, pool, or continuous mining-loop code while
  extending the verified M5 backend. M6 network code must remain inside the
  finite direct-node boundary and behind the CPU/crypto/live-send gates.
- Do not claim the production-shaped corpus is the canonical task or a
  production performance benchmark.
- Do not claim speedup, power, energy, profitability, or mining hashrate from
  any M2/M3/M4 correctness record or from the M5 raw timings.
- Do not redo the M3 K1 differential run unless source, toolchain, artifact, or
  contract changes require it.
- Do not redo the M4 physical run unless the M4 source, toolchain, artifact, or
  contract changes require it. Do not silently convert the one-column M4
  baseline into a batch or four-column experiment; use the checked-in M5
  runner/evidence for that comparison.

## Exact next task: finish the M6 gate or checkpoint it

Do not start M7. First rerun the inherited physical M1–M5 validation suite
after this recovery, then select and independently license-review a permissive
K12/FourQ-compatible provider. If an explicitly authorized current node and
safe signing material become available, exercise system-info interoperability
and only a CPU-verified, threshold-eligible submission with live opt-in.
Otherwise preserve `LIVE_SUBMISSION_NOT_EXERCISED`, keep M6 **IN PROGRESS**,
and record the external blocker. Do not weaken CPU verification, infer task
bytes, or claim profitability.

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
./scripts/run-m4-validation.sh
./scripts/run-m5-validation.sh
python3 -m json.tool docs/evidence/m5-batching-four-column.json
```

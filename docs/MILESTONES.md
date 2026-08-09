# Milestones

Milestones are strict engineering gates. A milestone may be marked COMPLETE only
when every acceptance criterion passes and `docs/AI_HANDOFF.md` is updated.
Static hypotheses, configured offload, and plausible source code are not
evidence.

**Current milestone:** M6 — Qubic direct-node integration
**Current status:** IN PROGRESS
**M0 status:** COMPLETE
**M3 status:** COMPLETE
**M4 status:** COMPLETE
**M5 status:** COMPLETE
**M6 status:** IN PROGRESS

## M0 — Repository bootstrap, research, and technical specification

### Objective

Establish a reproducible specification for current Qubic BPP9000, source/license
boundaries, CPU/NPU responsibilities, XDNA1 candidate kernels, correctness
contract, and benchmark method before implementation.

### Scope

- recover bootstrap state and inspect the required governance documents;
- pin current Qubic core, Qiner, official documentation, and relevant pool
  references by revision/path/license;
- document task, epoch/seed/threshold, candidate, score, validation, and
  direct-node submission lifecycle;
- document exact datatypes, dimensions, memory layout, operation counts, and
  serial/batch boundaries;
- rank XDNA1 kernels using the validated Hawk Point environment as a reference;
- design M1 scalar CPU reference and CPU/NPU differential contract;
- define M1–M11 gates and benchmark evidence.

### Dependencies

Existing bootstrap commits and authoritative upstream sources. No source code
or hardware dependency is required for the document audit.

### Deliverables

`docs/PROJECT_SPEC.md`, `docs/ARCHITECTURE.md`, `docs/UPSTREAM.md`,
`docs/DECISIONS.md`, `docs/TESTING.md`, `docs/BENCHMARKS.md`, and this file,
with the exact M1 handoff in `docs/AI_HANDOFF.md`.

### Tests/checks

Read all required bootstrap documents in order; verify upstream revisions and
task hashes; run `git status` and `git diff --check`; review documents for
contradictory algorithm, threshold, license, and status claims.

### Measurable acceptance criteria

- BPP9000 and its exact production constants are source-pinned.
- Task, seed/epoch, candidate/mutation, scoring, validation, and direct-node
  result lifecycle are documented with paths and revisions.
- Licensing/reuse restrictions are explicit for every implementation examined.
- CPU/NPU responsibility boundary and ranked kernels include shapes, datatypes,
  operation counts, transfer/synchronization risks, and four-column mapping.
- The scalar reference and exact differential matrix are actionable for M1.
- M1–M11 each have objective, dependencies, deliverables, tests, measurable
  gate, non-goals, and handoff requirement.
- No benchmark value or implementation claim is invented.
- **Current result:** COMPLETE. The direct-node path is canonical and its
  behavior is verified from the pinned core/Qiner sources. Official Qatum
  status statements conflict, but no Qatum wire behavior is guessed; Qatum is
  explicitly deferred as an optional future adapter.

### Non-goals

No miner, network adapter, CPU scorer, NPU runtime, kernel, benchmark, or M1
implementation.

### Handoff requirement

Record the exact first M1 task, current source revisions, the deferred Qatum
scope, and the instruction not to redo the source audit or start M2.

## M1 — CPU golden reference

### Status

**COMPLETE**

### Objective

Implement a readable, deterministic scalar CPU reference for the pinned BPP9000
task/scoring semantics.

### Dependencies

M0 documents and source pins. No NPU or live network dependency.

### Scope and deliverables

- standalone fixed-width domain structs for public key, mining seed, nonce,
  task header/topology/data, LUT/state, score result, and threshold predicates;
- little-endian task parser/packer and hash validation;
- deterministic random2 pool/draw abstraction and K12 boundary;
- scalar double-buffered recurrent tick and full window score;
- 100-step mutation/accept/rollback path;
- canonical nonce/seed/score validation;
- reproducible small vectors and production-shaped metadata vectors;
- readable unit and golden tests.

M1 implementation is under `src/bpp9000/` with a C++20/CMake library and
`tests/test_main.cpp`. The public reference boundary includes `parse_task`,
`serialize_task`, `score_window`, `score_lut`, `score_candidate`,
`mutate_lut`, `rollback_mutation`, `is_canonical_nonce`,
`is_valid_score`, and `is_good_score`. The random2/K12-dependent draws are
explicitly injected through a seed-aware `CandidateRandomSource`; M1 uses only
the deterministic fixture implementation.

### Tests

Run the complete parser and correctness matrix in `docs/TESTING.md`:
invalid trits, layout, zero/one/unknown values, mutation boundaries, timeout,
maximum score, deterministic random vectors, multiple candidates, multiple
iterations, and full reference path. Cross-check behavior against the pinned
core reference test/Qiner behavior without copying code.

### Measurable acceptance criteria

- Clean CMake configure/build and the documented test command pass on the
  development environment.
- The same fixed vector produces byte-identical score/state output on two
  independent runs.
- All required edge cases pass with zero unexplained failures.
- 100 generated and 10 production-shaped deterministic cases are reproducible
  from committed generator logic, metadata, and summary digests.
- Full BPP9000 score semantics, including 101 calls and timeout
  `0xffffffff`, are covered by tests.
- No AVX/NPU/network code is required for completion.

**M1 result:** COMPLETE. The build/test suite reports 8 groups and 361
assertions with zero failures. The corpus reports 100 generated cases and 10
production-shaped cases under generator `m1-v1`. No upstream task bytes or
Anti-Military-licensed implementation code was copied. No benchmark or
speedup claim was made.

### Non-goals

Performance tuning, live pool/node networking, XDNA execution, and AVX
optimization.

### Handoff requirement

Record the CPU API, vector IDs/hashes, exact test command/output, task source
revision, and the first M2 runtime-smoke task. M2 must pass before any M3 work
begins.

## M2 — XDNA1 runtime foundation

### Status

**COMPLETE**

### Objective

Prove a standalone, fail-closed XDNA1 execution path on target Hawk Point
hardware without mining-specific kernels.

### Dependencies

M1 CPU test command and a target system with the documented XDNA1 stack, or a
clearly recorded unavailable-hardware result.

### Scope and deliverables

- device identity/capability verifier;
- project-owned XRT/MLIR-AIE/IRON version pins;
- allocation, sync, dispatch, completion, and error handling;
- tiny deterministic smoke program;
- XRT/telemetry evidence capture and explicit CPU fallback labeling.

### Tests

Device-present and device-absent paths, repeated deterministic smoke outputs,
buffer sync/stride checks, runtime mismatch checks, and actual dispatch evidence.

### Measurable acceptance criteria

- A present target identifies `RyzenAI-npu1` and reports the exact runtime,
  kernel/firmware, XRT and compiler pins.
- The smoke program runs at least 100 repeated dispatches with zero output
  mismatches and no silent fallback.
- Logs/artifacts show genuine XRT/device dispatch; “configured NPU” alone does
  not count.
- Missing hardware fails with a classified diagnostic and no false NPU result.

### M2 result

The current Hawk Point host positively identifies `RyzenAI-npu1` / `aie2` at
`0000:06:00.1`, with `/dev/accel/accel0`, firmware `1.5.5.391`, XRT `2.26.0`,
and the current amdxdna/kernel pin recorded in `runtime-pins.json`. The
project-owned Iron/MLIR-AIE artifact is one-column `MLIR_AIE` code for the
non-mining transform `out[i] = 3 * in[i] + 7` over `int32[32]`. The acceptance
run completed 100 physical XRT dispatches with 100 exact CPU-oracle matches,
zero mismatches, zero runtime failures, 200 explicit H2D synchronizations, and
100 explicit D2H synchronizations. Evidence is saved at
`docs/evidence/m2-xdna-smoke.json`.

The negative selector, missing-artifact, invalid-artifact, invalid-buffer, and
zero-iteration paths fail closed with classified nonzero exits. No second-
generation device is present for a physical wrong-generation test; the probe
rejects known incompatible identity strings. Context/execution/output-mismatch
error paths are typed but were not artificially induced on the healthy device.

No BPP9000 NPU kernel, networking, production crypto, speedup, or profitability
claim was added. Four-column execution was not claimed; one column is the only
verified artifact mapping.

### Non-goals

BPP9000 scoring, network integration, multi-column performance, and
profitability.

### Handoff requirement

Record device/stack identity, commands, dispatch evidence, failure behavior,
and the exact primitive selected for M3.

## M3 — First NPU compute kernel

### Status

**COMPLETE**

### Objective

Port one isolated high-suitability deterministic BPP9000 primitive, initially
K1 recurrent tick or a smaller equivalent.

### Dependencies

M1 scalar oracle and M2 verified runtime.

### Scope and deliverables

- explicit input/output schema and pack/unpack;
- one AIE2 kernel/graph with no network/control state;
- single-batch and initial independent-lane execution;
- kernel-specific CPU/NPU tests and mismatch capture.

### Tests

All M1 primitive boundary vectors, 100 fixed cases, 1,000 seeded random cases
for the selected primitive, padded-layout round trips, and dispatch evidence.

### Measurable acceptance criteria

- Zero exact mismatches across the committed corpus and all required edge
  cases.
- At least 100 successful hardware dispatches with output evidence.
- Any unsupported hardware/runtime path is explicit and does not report an NPU
  result.
- Kernel timing is recorded only as an unqualified measurement, never as a
  speedup claim.

### M3 result

The selected primitive is the isolated BPP9000 K1 recurrent tick. The host
contract is exact: 64 logical `uint8_t` state trits, 46 LUT rows with 32-byte
storage stride and 27 logical entries per row, 64×3 `uint32_t` neighbors, and
46 ascending updated-neuron rows. Input roles are the 18 neurons absent from
the updated list and are copied unchanged; every updated row reads the prior
state buffer and selects one LUT entry with the unsigned base-3 index
`first + 3*second + 9*third`.

The physical artifact is a clean-room one-column AIE2/MLIR-AIE program. Its
single aligned input arena is 2,528 bytes with state/LUT/neighbors/updated-row
offsets `0/96/1568/2336`; the output BO is 96 bytes with a 64-byte logical
prefix. Padding is initialized and verified as semantically unused. The
combined arena was required by the one-column AIE DMA channel limit; it is not
a change to the logical M1 contract.

The final physical run used generator `m3-k1-v1`, 37 edge cases, 100 fixed
cases, and 1,000 random cases from seed `5562880460839399681`. It completed
1,139 physical dispatches, 1,139 exact logical matches, zero mismatches, and
zero runtime failures. It recorded 2,278 H2D and 1,139 D2H synchronizations.
M1 and M2 regressions and the missing-artifact, invalid-device-selector, and
wrong-manifest negative paths all passed. Evidence is in
`docs/evidence/m3-k1-differential.json`.

### Non-goals

Full candidate mutation search, live networking, and optimization based on
unverified throughput.

### Handoff requirement

Record kernel shape, datatype, tile/buffer placement, correctness corpus,
dispatch evidence, and the exact scope for M4. **M3 handoff result:** this is
one isolated tick per host dispatch; state residency across ticks, full scoring,
mutation, batching, four columns, networking, and performance remain outside
M3.

## M4 — Full CPU/NPU correctness path

### Status

**COMPLETE**

### Objective

Combine the selected NPU primitives into exact full-score execution while
retaining the CPU oracle as the submission authority.

### Dependencies

M1, M2, and M3.

### Scope and deliverables

- full window/scoring path or a justified fused subset sufficient for a
  candidate score;
- state reset, input feed, settling, timeout, output comparison, and score
  reduction;
- candidate ordering and status/error propagation;
- CPU canonical recomputation gate.

### Tests

All M1 tests, all required CPU/NPU matrix cases, 100 fixed cases, 1,000 seeded
random cases, multiple candidates/batches, at least one production-shaped task,
and saved reproducible mismatch artifacts.

### Measurable acceptance criteria

- Zero unexplained CPU/NPU mismatches, including score and timeout sentinel.
- At least one full candidate score agrees exactly with the CPU reference over
  all windows and mutation steps.
- A found/result candidate is rejected from submission if CPU recomputation
  differs.
- Actual dispatch evidence exists for the accelerated path.

### M4 result

The one-column Iron/MLIR-AIE artifact was exercised on the physical
`RyzenAI-npu1`/AIE2 device. The final run completed 1,000 repeated-tick cases,
100 one-window cases, 1,000 multi-window cases, 100 fixed cases, 1,000 seeded
random cases, 11 full-score cases including one production-shaped `T=8760,
W=672` case, and two independent 101-score-call candidate lifecycles. It
completed 13,460 physical dispatches with 13,460 successful dispatches,
zero CPU/NPU mismatches, zero runtime failures, 26,920 H2D synchronizations,
and 13,460 D2H synchronizations. The differential record contains 213 score
runs, 12,460 window comparisons, and 4,313 exact verification comparisons;
candidate score calls and candidate window comparisons are recorded
separately. The timeout sentinel matched exactly wherever exercised.

M1 digests, the M2 100-dispatch smoke, and the M3 37-edge/100-fixed/1,000
random K1 differential all remained green. Missing-artifact, invalid-device,
and wrong-manifest runtime negatives passed, as did pure M4 invalid-trit,
invalid-topology, malformed-sequence, invalid-window, bounded-tick, timeout,
and score-mismatch checks. Evidence is in
`docs/evidence/m4-full-score-differential.json`.

### Non-goals

Pool protocol, four-column tuning, and performance claims.

### Handoff requirement

Record the full boundary, mismatch corpus, verification-gate behavior, and
the exact one-column baseline for M5. M5 owns batching and four-column
experiments; no M5 performance work is included in M4.

## M5 — Batching and four-column execution

### Status

**COMPLETE**

### Objective

Amortize transfer/scheduling and evaluate independent work partitioning across
the four AIE2 columns.

### Dependencies

M4 exact full-score path and M2 device evidence.

### Scope and deliverables

- candidate- and/or window-batch schema;
- persistent task/topology/LUT/state buffers where correct;
- batch-size sweep and four-column mappings;
- H2D/D2H, launch, kernel, and reduction accounting;
- rejection record for mappings that do not help.

### Tests

Exact differential tests at every selected batch size, candidate ordering,
column-shard isolation, repeated epoch/task reuse, and device activity evidence.

### Measurable acceptance criteria

- Zero correctness regressions.
- Every claimed active column has hardware evidence or is labeled unverified.
- At least three documented batch sizes are compared with raw timing and
  transfer breakdown.
- The selected mapping either demonstrates a measured improvement over the
  M4 baseline or records a technically supported rejection; no improvement is
  assumed.
- Best batch/column configuration and its limits are checked in as evidence.

### M5 result

M5 selected one complete independent candidate/window pair as the batch work
unit. The physical host run used 16 deterministic pairs, two warm-ups, and
five measured repeats for every accepted `(batch_size, columns)` artifact.
The matrix `(1,1)`, `(2,1)`, `(4,1)`, `(2,2)`, `(4,2)`, `(8,2)`, `(4,4)`,
`(8,4)`, and `(16,4)` completed with 80/80 exact measured item matches per
configuration, zero mismatches, and zero runtime failures. The runner also
verified ordered, reversed, and `A,A,B,A` lane patterns, per-item reset,
and mutate/dispatch/rollback/dispatch LUT visibility.

The identical-work M4 reference measurement used 80 one-item dispatches,
160 H2D syncs, 80 D2H syncs, and a 2.479492 ms median wall time (p95
3.015180 ms). The selected M5 artifact is batch 16/four columns: five
measured dispatches, 10 H2D syncs, five D2H syncs, and a 1.067016 ms median
wall time (p95 1.451890 ms). M5 H2D bytes are 1,249,280 versus the M4
1,246,800 because the fixed M5 input stride retains 31 explicit padding bytes
per item; D2H bytes are equal. These are recorded as raw timings, with no
profitability or hashrate claim.

Generated `npu1_1col`, `npu1_2col`, and `npu1_4col` placement metadata, lane
ranges, artifact SHA-256 values, instruction SHA-256 values, runtime UUIDs,
and physical XRT completion evidence are checked in through
`docs/evidence/m5-batching-four-column.json`. The reproducible full gate is
`./scripts/run-m5-validation.sh`. M6 is **IN PROGRESS** and owns only the
direct-node protocol integration around this verified backend; M7 remains
out of scope.

### Non-goals

Changing the algorithm, weakening CPU verification, or adding an unpinned pool
protocol.

### Handoff requirement

Record the chosen batch/column contract and transfer/kernel evidence for M6/M7.

## M6 — Qubic direct-node and optional pool integration

### Status

**IN PROGRESS** — clean-room framing, system-info parsing, WorkContext
freshness, CPU/NPU submission gates, deterministic solution serialization,
bounded transport/reconnect, mock integration, secret-redacted runtime
configuration, and an optional pinned K12/FourQ-compatible provider are
implemented. The provider passes RFC, synthetic Qubic SchnorrQ, public-key,
shared-key, gamming-key, and gamma-stream KATs. No authorized live endpoint or
submission secret was configured; live interoperability is therefore not
exercised. M6 is not complete.

### Objective

Implement and validate the versioned, authorized direct-node work-acquisition
and result-submission path without coupling networking to the accelerator.
Treat any pool protocol as an optional adapter after direct-node support works.

### Dependencies

M1 for canonical protocol objects; M4/M5 for a verified compute backend;
a stable upstream protocol revision for each adapter.

### Scope and deliverables

- direct-node system-info request/response and task compatibility checks;
- epoch/seed/threshold freshness and stale-work cancellation;
- framed signed/encrypted solution broadcast and bounded reconnect;
- independently reviewed optional K12/FourQ provider with deterministic KATs;
- captured/mock protocol vectors and authorized interoperability;
- an optional pool adapter only for a separately pinned stable protocol. Qatum
  and QLI behavior must not be conflated.

### Tests

Header/frame parsing, system info fields, signature/gamma payload construction,
bad score/seed/nonce/threshold rejection, reconnect/timeouts, secret-redaction,
mock packets, and authorized endpoint tests.

### Measurable acceptance criteria

- Direct-node adapter passes all committed vectors and demonstrates one
  authorized current-node request/submit interoperability run, or records the
  external endpoint as unavailable without claiming pass. The optional crypto
  provider must pass its committed K12/FourQ KATs before any live attempt.
- Every submitted score is CPU-recomputed and exact.
- The adapter rejects stale/unknown algorithm/task context and never logs
  signing keys/tokens.
- Direct-node acceptance is required first. Qatum/pool acceptance is optional
  and deferred until a stable protocol revision, compatible license, and wire
  vectors are pinned and independently reviewed. M1 through M5 must not depend
  on Qatum or any pool.

### Non-goals

Pool reverse engineering, secret handling in tests, and NPU optimization.

### Handoff requirement

Record protocol revisions, captured vectors, endpoint assumptions, direct-node
acceptance status, and whether the optional pool adapter remains deferred.

## M7 — Complete mining pipeline

### Objective

Connect current work context, CPU candidate control, verified compute,
canonical verification, and authorized submission.

### Dependencies

M4, M5, and the completed direct-node gate in M6; pool is optional only if
M6 explicitly pinned it.

### Scope and deliverables

- lifecycle supervisor for epoch/seed changes;
- candidate queue/control and CPU/NPU scheduling;
- CPU verification before submit;
- explicit NPU failure/fallback policy;
- observable result/rejection/error reporting.

### Tests

End-to-end mock node, authorized interoperability, stale seed during batch,
NPU failure, CPU fallback labeling, submission rejection, shutdown, and
reconnect.

### Measurable acceptance criteria

- One full controlled run reaches candidate scoring and exact CPU verification
  under a current seed/task context.
- No stale or mismatched candidate is submitted.
- Every path is labeled CPU, NPU, or hybrid with dispatch evidence.
- All injected network/NPU failures are surfaced and recovered or stopped
  explicitly.
- No profitability or throughput claim is required for this gate.

### Non-goals

Unmeasured optimization, unpinned pool support, and release packaging.

### Handoff requirement

Record an end-to-end run artifact, failure matrix, and optimization baseline.

## M8 — Performance optimization

### Objective

Optimize stable, correct pipeline components using measured evidence.

### Dependencies

M7 and the benchmark protocol.

### Scope and deliverables

- layout/tiling/fusion/buffer reuse/queue depth/batch/overlap experiments;
- CPU/NPU scheduling and thermal/power controls;
- before/after raw benchmark artifacts;
- rejected optimization log.

### Tests

Full M1/M4 correctness suite after every accepted change; regression and
resource checks.

### Measurable acceptance criteria

- Every accepted optimization has a before/after workload identity and raw
  metrics.
- Zero correctness regressions and no weakened error detection.
- At least one optimization decision is reproducible from a clean checkout.
- No result is labeled a speedup unless the CPU baseline and configuration are
  identical in logical work.

### Non-goals

Algorithm changes, unreviewed upstream code copying, and cherry-picked metrics.

### Handoff requirement

Record accepted/rejected changes, measurement artifacts, and M9 comparison plan.

## M9 — Comparative benchmarking

### Objective

Measure CPU reference, CPU optimized, NPU, and CPU+NPU hybrid under identical
correctness-controlled workloads.

### Dependencies

M8 and a passing correctness suite.

### Scope and deliverables

- production task/corpus identity;
- candidate scores/sec and algorithm-native work/sec;
- batch latency, transfer/kernel time, CPU/RAM, NPU evidence, errors;
- power/energy/work-Joule only from named measurement sources.

### Tests

Repeated warm/cold methodology, correctness preflight, telemetry availability,
failed-run classification, and raw artifact review.

### Measurable acceptance criteria

- All four placements are measured where technically available, or each
  unavailable placement has a recorded reason.
- Median/p95 latency, throughput, batch, CPU utilization, RAM, transfer,
  kernel, correctness/error counts, and NPU evidence are reported.
- Power/energy claims include source, interval, units, and limitations.
- No unmeasured cell is converted into an estimate or profitability claim.

### Non-goals

Changing code solely to improve a single unrepeatable run.

### Handoff requirement

Record the comparison table, raw artifacts, limitations, and release candidate
benchmark summary.

## M10 — Endurance and recovery

### Objective

Prove sustained correctness, resource bounds, epoch/job handling, and recovery.

### Dependencies

M7 for lifecycle correctness; M8/M9 for the selected release configuration.

### Scope and deliverables

- minimum 24-hour release endurance run;
- epoch/seed changes, reconnects, NPU/XRT failures, queue/device restart;
- memory/resource and graceful shutdown evidence.

### Tests

Injected and real authorized network interruption, stale work, device error,
submission rejection, restart, and long-run correctness checks.

### Measurable acceptance criteria

- The defined 24-hour release run completes with zero silent correctness
  errors and all failures classified.
- Resource high-water marks remain within documented limits and show no
  unbounded growth.
- At least one recovery of each supported failure class is evidenced, or the
  miner stops safely and documents the unsupported recovery.
- Final results remain CPU-canonical before submission.

### Non-goals

New optimization or protocol experimentation during the endurance run.

### Handoff requirement

Record run duration, hardware/software identity, event log, resource metrics,
and unresolved recovery limits.

## M11 — Packaging and release

### Objective

Publish a reproducible experimental release with explicit support and license
boundaries.

### Dependencies

M10 and all prior gates.

### Scope and deliverables

- CLI/configuration and endpoint/identity setup;
- clean install/build and environment verifier;
- supported Fedora/XDNA1/XRT/MLIR-AIE/IRON matrix;
- version reporting, notices, license audit, safety documentation;
- benchmark and endurance artifacts;
- release tag and final handoff.

### Tests

Clean checkout build, device verification, CPU-only test suite, hardware smoke
when available, configuration validation, secret-redaction review, and package
install/uninstall.

### Measurable acceptance criteria

- Clean build/install succeeds on the documented target stack.
- The verifier reports the expected device/runtime or fails clearly.
- A user can configure endpoint/identity without editing source.
- Release docs distinguish direct-node support from any pool support and label
  experimental status.
- Upstream notices/license decisions are complete and no unlicensed code was
  imported.
- Release artifacts identify exact commit, task, corpus, benchmark, and
  endurance revisions.

### Non-goals

Profitability guarantees, unsupported hardware, and undocumented pool claims.

### Handoff requirement

Update all docs for the tagged state, record the release verification commands,
and leave the next maintenance task explicit.

# Milestones

Milestones are strict engineering gates. A milestone may be marked COMPLETE only
when every acceptance criterion passes and `docs/AI_HANDOFF.md` is updated.
Static hypotheses, configured offload, and plausible source code are not
evidence.

**Current milestone:** M1 — CPU golden reference
**Current status:** NOT STARTED
**M0 status:** COMPLETE

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

**NOT STARTED**

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

### Tests

Run the complete parser and correctness matrix in `docs/TESTING.md`:
invalid trits, layout, zero/one/unknown values, mutation boundaries, timeout,
maximum score, deterministic random vectors, multiple candidates, multiple
iterations, and full reference path. Cross-check behavior against the pinned
core reference test/Qiner behavior without copying code.

### Measurable acceptance criteria

- Clean build and one documented test command pass on the supported Fedora
  development environment.
- The same fixed vector produces byte-identical score/state output on two
  independent runs.
- All required edge cases pass with zero unexplained failures.
- At least 100 generated and 10 production-shaped deterministic cases are
  reproducible from committed input/corpus metadata.
- Full BPP9000 score semantics, including 101 calls and timeout
  `0xffffffff`, are covered by tests.
- No AVX/NPU/network code is required for completion.

### Non-goals

Performance tuning, live pool/node networking, XDNA execution, and AVX
optimization.

### Handoff requirement

Record the CPU API, vector IDs/hashes, exact test command/output, task source
revision, and the first M2 runtime-smoke task. Do not begin M3.

## M2 — XDNA1 runtime foundation

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

### Non-goals

BPP9000 scoring, network integration, multi-column performance, and
profitability.

### Handoff requirement

Record device/stack identity, commands, dispatch evidence, failure behavior,
and the exact primitive selected for M3.

## M3 — First NPU compute kernel

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

### Non-goals

Full candidate mutation search, live networking, and optimization based on
unverified throughput.

### Handoff requirement

Record kernel shape, datatype, tile/buffer placement, correctness corpus,
dispatch evidence, and the exact scope for M4.

## M4 — Full CPU/NPU correctness path

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

### Non-goals

Pool protocol, four-column tuning, and performance claims.

### Handoff requirement

Record the full boundary, mismatch corpus, verification-gate behavior, and
the batch/column experiments for M5.

## M5 — Batching and four-column execution

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

### Non-goals

Changing the algorithm, weakening CPU verification, or adding an unpinned pool
protocol.

### Handoff requirement

Record the chosen batch/column contract and transfer/kernel evidence for M6/M7.

## M6 — Qubic direct-node and optional pool integration

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
  external endpoint as unavailable without claiming pass.
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

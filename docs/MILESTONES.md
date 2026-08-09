# Milestones

Milestones are strict engineering gates. A milestone may be marked COMPLETE only when all acceptance criteria pass and `docs/AI_HANDOFF.md` is updated.

## M0 — Repository bootstrap and technical specification

### Objective
Establish authoritative knowledge of the current Qubic mining algorithm/protocol, upstream licensing boundaries, CPU/NPU responsibility split, architecture, and measurable acceptance criteria before implementation.

### Scope
- audit current authoritative Qubic mining sources;
- record versions/commits/tags/files in `docs/UPSTREAM.md`;
- document mining job lifecycle, candidate generation/mutation, scoring/validation and result submission;
- identify datatypes, dimensions, repeated operations and memory-access patterns;
- identify XDNA1 kernel candidates;
- audit upstream licenses and clean-room requirements;
- refine architecture and test strategy.

### Acceptance criteria
- current algorithm/protocol documented from authoritative sources;
- important claims traceable through `docs/UPSTREAM.md`;
- licensing boundary documented;
- CPU/NPU responsibility split defined;
- candidate NPU kernels analyzed;
- architecture updated;
- M1 has an exact actionable starting task;
- no mining functionality is falsely claimed.

### Non-goal
Do not implement the miner.

---

## M1 — CPU golden reference

### Objective
Implement a deterministic trusted CPU reference for the exact compute/scoring workload selected in M0.

### Scope
- exact data structures and arithmetic semantics;
- deterministic input vectors;
- CPU implementation independent from NPU code;
- test-vector generation/verification against authoritative behavior when possible.

### Acceptance criteria
- deterministic tests pass;
- golden vectors are checked in or reproducibly generated;
- edge cases and overflow/rounding semantics documented;
- at least one authoritative interoperability/reference check exists;
- no XDNA acceleration is required.

### Non-goal
Performance tuning.

---

## M2 — XDNA1 runtime foundation

### Objective
Establish a minimal, verifiable XDNA1 execution path independent of the full mining workload.

### Scope
- XRT/MLIR-AIE/IRON integration as selected by M0;
- device detection;
- buffer allocation/transfer;
- dispatch;
- tiny deterministic hardware smoke kernel;
- hardware capability/version reporting.

### Acceptance criteria
- target device is positively identified as XDNA1 / `RyzenAI-npu1` on hardware;
- a deterministic NPU kernel executes successfully;
- output matches CPU expectation;
- logs/telemetry prove actual NPU dispatch;
- failure mode is explicit when XDNA1 is unavailable.

---

## M3 — First mining compute kernel on NPU

### Objective
Port one high-value compute primitive from the CPU golden workload to XDNA1.

### Scope
- one isolated kernel selected from M0 analysis;
- deterministic host/NPU interface;
- correct datatype/overflow/rounding behavior;
- kernel-specific tests.

### Acceptance criteria
- NPU output matches M1 CPU reference across representative vectors;
- actual XDNA dispatch is evidenced;
- no silent fallback;
- initial latency/throughput recorded without unsupported performance claims.

---

## M4 — Full CPU/NPU correctness path

### Objective
Complete the accelerated compute/scoring path required for a mining candidate and prove agreement with the CPU reference.

### Scope
- remaining required NPU kernels;
- end-to-end candidate/scoring execution excluding live pool/node integration;
- differential testing.

### Acceptance criteria
- CPU/NPU agreement over a large deterministic corpus;
- randomized/fuzz-style differential tests where appropriate;
- boundary and adversarial arithmetic cases pass;
- failures reproduce with saved inputs;
- no correctness tolerance is widened merely to obtain a pass.

---

## M5 — Batching and four-column XDNA1 execution

### Objective
Improve throughput by amortizing transfers and exploiting the 4-column AIE2 array where useful.

### Scope
- batch design;
- persistent/reused buffers;
- multi-column mapping;
- scheduling/pipelining experiments;
- transfer/computation breakdown.

### Acceptance criteria
- correctness remains identical to M4;
- benchmark demonstrates measured improvement or documents why a technique was rejected;
- best batch size/configuration recorded;
- host/NPU transfer cost quantified;
- four-column claims backed by hardware evidence.

---

## M6 — Qubic network / pool integration

### Objective
Implement current protocol-compatible work acquisition and submission without coupling networking to accelerator internals.

### Scope
- endpoint/configuration handling;
- job/work acquisition;
- protocol parsing/serialization;
- reconnect/timeouts;
- share/result submission;
- protocol-level tests.

### Acceptance criteria
- current authoritative protocol version supported;
- captured/mock vectors pass;
- live interoperability demonstrated on an authorized endpoint/test environment when available;
- malformed input fails safely;
- secrets are not logged.

---

## M7 — Complete mining pipeline

### Objective
Connect network work acquisition, candidate generation/control, XDNA compute, CPU verification and submission into one functioning miner.

### Acceptance criteria
- complete pipeline runs on target hardware;
- accepted/valid result behavior is demonstrated where practical;
- stale/rejected work is handled correctly;
- NPU failures do not produce silent bad submissions;
- CPU fallback policy, if any, is explicit and never masquerades as NPU execution.

---

## M8 — Performance optimization

### Objective
Optimize only after correctness and complete pipeline behavior are stable.

### Candidate work
- memory layout;
- kernel fusion;
- tiling;
- buffer reuse;
- queue depth;
- batch size;
- CPU/NPU overlap;
- vectorization;
- reduced synchronization.

### Acceptance criteria
- each accepted optimization preserves correctness tests;
- before/after benchmark evidence is recorded;
- regressions and rejected experiments are documented;
- optimization does not weaken failure detection.

---

## M9 — Comparative benchmarking

### Objective
Measure whether XDNA1 provides useful performance and/or energy efficiency.

### Required comparison where feasible
- CPU reference;
- NPU accelerated;
- hybrid CPU+NPU;
- other relevant backend only when available and methodologically fair.

### Acceptance criteria
- identical workload conditions documented;
- warm-up and repetitions documented;
- throughput and latency reported;
- correctness/error counts reported;
- RAM and available device telemetry reported;
- power/energy reported only when measurement is reliable;
- no profitability claim without contemporaneous external economics and a separate calculation.

---

## M10 — Endurance and recovery

### Objective
Prove sustained correctness and recoverability.

### Scope
- long-running workload;
- repeated job switches;
- network interruptions;
- NPU/XRT error recovery;
- resource leak detection;
- graceful shutdown/restart.

### Acceptance criteria
- defined endurance run completes with zero silent correctness errors;
- memory/resource behavior remains bounded;
- recovery paths are tested;
- all errors are surfaced and classified.

---

## M11 — CLI, packaging and release

### Objective
Produce a reproducible, documented experimental release.

### Scope
- CLI/configuration;
- environment verification;
- install/build scripts;
- version reporting;
- documentation;
- release checklist;
- license/notice files;
- reproducible benchmark references.

### Acceptance criteria
- clean install/build on the documented target stack;
- hardware verification command works;
- user can configure endpoint/identity/settings without editing code;
- release clearly labels experimental status and supported hardware/software boundary;
- license and upstream notices are complete;
- handoff/release documentation reflects the exact tagged state.

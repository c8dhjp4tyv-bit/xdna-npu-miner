# Engineering Decisions

Record material decisions here so later agents do not silently reverse them.

## D-001 — Standalone repository

**Status:** Accepted

The mining project is separate from `hawkpoint-npu-llm`.

Reason: mining has different protocol, security, benchmark, release and
optimization concerns. XDNA1 knowledge may be reused conceptually, but
repository state and dependencies remain independent.

## D-002 — Qubic BPP9000 is the current target

**Status:** Accepted for M0; implementation remains gated

Qubic core `v1.301.3` identifies `Bpp9000` as the active algorithm
(`nonce[0] == 1`). `Neuraxon` is reserved and not implemented in that
revision. M1 should target BPP9000 exactly unless a newer pinned upstream
revision changes the algorithm before implementation starts.

Evidence: `qubic/core` commit
`a83f935406cd006b5b1a94971139e74d410ecb6d`, especially
`src/mining/score_common.h`, `src/mining/score_bpp9000.h`,
`src/public_settings.h`, and `src/mining/task_file.h`.

## D-003 — CPU golden reference precedes NPU mining kernels

**Status:** Accepted

No production mining compute path may be ported to XDNA1 until a deterministic
scalar CPU reference exists for the same arithmetic semantics. M1 is the next
implementation task; M0 contains no scorer or kernel code.

## D-004 — No cosmetic NPU mode

**Status:** Accepted

NPU mode requires evidence of actual XDNA1 dispatch. An unavailable device,
failed load, dispatch error, or CPU fallback must be explicit and must not be
counted as NPU throughput.

## D-005 — Repository is the agent memory

**Status:** Accepted

AI agents may change frequently and may have zero prior conversation context.
`AGENTS.md`, `docs/AI_HANDOFF.md`, and milestone documents are authoritative
for continuation.

## D-006 — Direct node is canonical; pool protocols are adapters

**Status:** Accepted

The canonical protocol path for this project is the standard direct-node path:
system-info work context plus signed/encrypted solution broadcast and CPU
canonical verification. The direct-node behavior verified from the pinned
Qubic core/Qiner sources is sufficient for M0.

M1 through M5 have **zero dependency** on Qatum or any mining pool. M6 must
implement and validate direct-node integration first. Qatum/pool integration is
optional after the direct-node path works and may be added only after an
authoritative, sufficiently complete, versioned wire specification or
implementation is pinned and independently reviewed. Pool-specific
proprietary protocols are adapters, not part of the mining/scoring core. No
guessed Qatum wire format may be added.

## D-007 — Licensing is a gate, not cleanup work

**Status:** Accepted

Upstream Qubic core and Qiner use a custom Anti-Military License with no SPDX
identifier in the inspected license files. The grant includes field-of-use
restrictions incompatible with assuming ordinary permissive reuse. QLI client
source inspected for pool behavior has no license file. Therefore Qubic source
is reference-only and the miner will use a clean-room reimplementation unless
a separate legal review grants reuse.

The only permissive exception found in core is `LICENSE-MIT.md`, which names
the `uint128` files only; it does not license the scorer or node code. Qiner's
Catch2 dependency is listed as BSL-1.0 in `NOTICE.md`, but that does not
license Qiner.

## D-008 — Core production task and threshold outrank Qiner fixtures

**Status:** Accepted

The core production task at `core/data/bpp9000.task` has pinned topology/data
hashes and a default threshold of 3,838 in `v1.301.3`. Qiner
`v1.302.3` uses an example task with different hashes and a compile-time
threshold of 6,469 (`(numberWindows-1)*4/5`) marked as adjustable. This is a
reference/example discrepancy, not evidence of two active algorithms.

The future miner must hash-check the task it uses and obtain the runtime
threshold from system information/operator configuration. It must not hard-code
Qiner's 6,469 threshold or treat the Qiner example task as canonical network
data.

## D-009 — CPU/NPU boundary is recurrent compute only

**Status:** Accepted for architecture

The NPU candidate boundary is the repeated recurrent LUT/tick work, preferably
with complete independent candidates or windows resident per batch. Network,
epoch freshness, task validation, nonce/mutation control, crypto/signing,
accept/rollback, and final score verification remain CPU-owned. A NPU result
cannot authorize submission without an exact CPU verification gate.

K1 recurrent tick is the first high-suitability experiment. K2 fused scoring is
medium suitability. K3 full mutation/search is optional and not required for
the first kernel. K4 reduction and K5 random2/K12 are not standalone offload
targets absent measurements.

## D-010 — Four columns partition independent work

**Status:** Accepted for initial hypothesis

The four AIE2 columns should first own complete candidate/window shards,
approximately `B/4` each. Splitting a single recurrent candidate across
columns would require synchronization at every tick and is rejected as the
initial mapping. This is a testable hypothesis, not a utilization or speed
claim.

## D-011 — Exact integer contract, no tolerance

**Status:** Accepted

BPP9000 trits, LUT entries, states and packed task values use explicit
`uint8_t`; topology indices use `uint32_t`; mutation seeds use
`uint64_t`; scores and tick counters use `uint32_t`. The timeout is
`0xffffffff`. CPU/NPU outputs must match exactly. Value `2` is UNKNOWN,
not signed `-1`; invalid negative/out-of-range input must be rejected.

## D-012 — No benchmark claims in M0

**Status:** Accepted

M0 records operation counts and suitability hypotheses only. Throughput,
latency, NPU activity, power, work/Joule and speedup remain unmeasured until
correctness passes and the benchmark protocol in `docs/BENCHMARKS.md` is
followed.

## D-013 — Reference XDNA environment is informational

**Status:** Accepted

`hawkpoint-npu-llm` and the pinned MLIR-AIE checkout were inspected for
validated Fedora/XDNA1/XRT/IRON patterns. This repository remains standalone;
it must select and pin its own dependencies in M2 and must not copy source or
couple build layout by assumption.

## D-014 — Qatum uncertainty does not block direct-node M0

**Status:** Accepted

Direct-node algorithm, task, seed/threshold lifecycle, scoring, validation and
submission semantics are source-pinned. Official Qubic sources conflict about
whether Qatum is live or in development, and no sufficiently complete,
authoritative Qatum wire contract can be pinned from that status information.
This is an explicit deferred scope decision, not permission to invent a
protocol and not an M0 blocker. M1 through M5 proceed without Qatum or pool
dependencies; M6 starts with direct-node integration. Qatum remains optional
until the specification/implementation gate in D-006 passes.

## D-015 — Pinned upstream authority and re-check policy

**Status:** Accepted

Qubic core at `a83f935406cd006b5b1a94971139e74d410ecb6d` is the canonical source
of consensus constants and validation behavior. Qiner at
`11fb18a6f4944bb55fe103d3f263cb5d31e00200` is a behavioral/reference aid only;
its example task and example threshold are not production consensus truth.
Future agents must re-check current upstream revisions before implementing
network-facing behavior because Qubic mining is actively changing. No
Anti-Military-licensed upstream source may be copied into this repository.

## Historical decisions retained

The original bootstrap decisions remain valid:

- correctness before performance;
- no silent CPU fallback;
- milestone completion requires evidence;
- mining is only for hardware and endpoints the operator is authorized to use;
- profitability is outside core correctness and benchmark claims.

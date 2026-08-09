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

## D-016 — C++20 standalone scalar implementation

**Status:** Accepted for M1

The CPU oracle is a small standard-library-only C++20 library built with
CMake. It uses fixed-width integers and explicit byte serialization. The M1
reference has no compiler intrinsics, SIMD, AVX, NPU, network, or external
runtime dependency.

Reason: later CPU/NPU differential tests need a readable implementation whose
state and byte boundaries can be audited without inheriting an upstream
implementation structure.

## D-017 — K12/random2 remains an injectable boundary in M1

**Status:** Accepted for M1

The candidate API requires a seed-aware `CandidateRandomSource` for root-LUT
bytes and mutation words. The interface records explicit draw order and
64-byte padding, matching the documented production boundary. M1 supplies
only a deterministic fixture source; it does not implement or copy
KangarooTwelve, random2, or Qubic crypto code. A reviewed production provider
must be selected before network-facing use.

Reason: M1's acceptance gate is scorer correctness and reproducibility, while
the upstream crypto implementation is Anti-Military licensed and the target
project has not completed a file-level crypto/license selection.

## D-018 — Corpus tiers and production-shaped scope

**Status:** Accepted for M1

The committed corpus is generated from metadata and deterministic generators,
not from Qubic or Qiner task bytes. One hundred small cases run the full
candidate search and assert 101 score calls. Ten production-shaped cases use
the exact production dimensions and serialized length, parse/hash-check their
independent fixture metadata, and run one complete production-width window.
This avoids claiming an impractical ten-fold full production benchmark while
still exercising production layout and recurrent semantics. The canonical
production task remains an external, hash-verified input.

## D-019 — Canonical score predicates include completion metadata

**Status:** Accepted for M1

`is_valid_score` requires a settled, non-timeout score whose evaluated-window
count equals the caller's expected window count and whose failure count is no
greater than that count. This prevents a partial score result from being
treated as a complete candidate result. The timeout sentinel remains exactly
`0xffffffff`.

## D-020 — M2 uses a standalone XRT host boundary and one-column smoke

**Status:** Accepted for M2

The runtime foundation is owned by this repository under `src/xdna/` and uses
the project-selected MLIR-AIE/IRON artifact plus the XRT C++ host API. It does
not depend on `hawkpoint-npu-llm`. The first artifact uses one AIE2 column and
the non-mining transform `out[i] = 3 * in[i] + 7` over 32 `int32` values.

Reason: M2 proves real allocation, synchronization, hardware-context creation,
device arithmetic, completion, and exact host verification with the smallest
auditable workload. Four-column mapping and BPP9000 compute are later gates,
not prerequisites to a truthful runtime smoke.

## D-021 — Capability success requires physical identity and pinned versions

**Status:** Accepted for M2

`SUPPORTED_XDNA1` requires an XRT-opened device whose identity is corroborated
by `xrt-smi` as `RyzenAI-npu1`/AIE2 with firmware and XRT version evidence.
`runtime-pins.json` records the current observed stack, and
`scripts/verify-xdna1.sh` returns `RUNTIME_VERSION_MISMATCH` when those pins do
not match. An environment variable, device node, compilation result, or
successful CPU calculation is not sufficient evidence.

Reason: the related environment used a different kernel suffix, so silently
inheriting its pin would not establish the current machine's identity.

## D-022 — M3 starts with K1, but M2 does not implement it

**Status:** Accepted for the M2-to-M3 handoff

The first M3 primitive is one recurrent LUT tick over independent
candidate/window lanes: 64-byte state, 46 rows of 32-byte LUT storage, and
three `uint32_t` neighbors per updated neuron. It is selected because the
state/LUT layout is explicit, topology is reusable, outer lanes are
independent, and host round trips can be avoided. M2 implements no recurrent
tick, LUT, scoring, mutation, or BPP9000 device behavior.

Reason: selecting the boundary from verified runtime constraints is required;
implementing the kernel before the M2 evidence gate would collapse milestones.

## D-023 — M3 K1 is an exact one-tick, one-column physical primitive

**Status:** Accepted for M3

M3 implements only one isolated recurrent LUT tick. The logical contract is
the M1 contract: 64 unsigned state trits, 46 LUT rows with 32-byte storage
stride and 27 logical entries, 64×3 serialized neighbor indices, and 46
ascending updated-neuron rows. The 18 neurons absent from the updated list are
held; updated rows read the prior state and commit to a separate next-state
buffer. `2` remains UNKNOWN and is never reinterpreted as signed `-1`.

The artifact uses one AIE2 column and the host performs exact CPU/NPU
differential comparison. A physical dispatch is not a score, mutation step,
full-window result, or performance benchmark. M3 completion does not authorize
networking, submission, or CPU fallback.

Reason: this is the smallest useful BPP9000 device computation that can be
audited against the already verified scalar oracle and physically demonstrated
without guessing protocol behavior.

## D-024 — K1 uses one combined aligned input arena; state residency waits for M4

**Status:** Accepted for M3

The K1 artifact packs state, LUT, neighbors, and updated-neuron rows into one
2,528-byte aligned input arena at offsets `0`, `96`, `1,568`, and `2,336`, with
a 96-byte output BO and 64-byte logical output. The initial independent-input
artifact shape was rejected by the one-column AIE compiler/shim because it
exceeded the available DMA channel budget. Combining the fields preserves the
logical M1 schema and is covered by padding-isolation tests.

The host deliberately transfers and returns one isolated tick per dispatch;
it does not claim device-resident recurrent state. Persistent multi-tick state,
candidate batching, four-column mapping, and full score composition are
deferred to M4/M5 where their synchronization and correctness contracts can
be measured.

## Historical decisions retained

The original bootstrap decisions remain valid:

- correctness before performance;
- no silent CPU fallback;
- milestone completion requires evidence;
- mining is only for hardware and endpoints the operator is authorized to use;
- profitability is outside core correctness and benchmark claims.

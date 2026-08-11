# Engineering Decisions

Record material decisions here so later agents do not silently reverse them.

## D-044 — Pearl P2-P11 use one continuous execution shot with strict gates

**Status:** Accepted for the Pearl full-project branch

Pearl is implemented on `feat/pearl-full-miner-one-shot` from the exact P1
checkpoint `ba286d5770c93290a38784f89ae75cea87867b25`. P2 through P11 proceed
without turn-by-turn handback, but remain sequential acceptance gates. A
blocked physical device, upstream service, operator-owned public payout
address, license boundary, or manual system intervention is recorded exactly;
it is never converted into PASS. Independent work continues around an external
blocker. Qubic and Qatum remain frozen.

Reason: the execution mode changes scheduling, not correctness, evidence,
license, security, or milestone semantics.

## D-041 — Pearl is a separate active track; Qubic is frozen reference

**Status:** Accepted for Pearl P0/P1

Pearl (PRL) research proceeds under `docs/pearl/` and branch
`feat/pearl-p1-cpu-golden`. The existing Qubic M0–M6 work, source boundaries, Qatum
decision, identities, and evidence are preserved as a frozen reference and
must not be continued or rewritten by Pearl work. Pearl P0 was documentation
only; P1 adds only the isolated clean-room CPU oracle and corpus. Unclear
upstream components remain clean-room-only, and the CPU remains authoritative
for network, proof, verification, and submission.

Reason: the Pearl mining path, licenses, and protocol are materially different
from Qubic. Separate records prevent accidental protocol or source reuse and
make the XDNA1 feasibility gate auditable.

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

## D-025 — M4 uses an auditable one-window device boundary

**Status:** Accepted for M4

M4 extends the clean-room K1 artifact with repeated-tick and one-window score
modes, but keeps full-window iteration and score reduction on the host. Each
window is independently packed, physically dispatched, and compared with the
M1 scalar `score_window` result before the host accumulates the CPU/NPU score.
This preserves an observable mismatch boundary for state reset, input timing,
settling, timeout, and target indexing while avoiding an opaque full-miner
kernel. Mutation materialization, accept/rollback, and candidate authority
remain CPU-owned.

Reason: M4's gate is exact full-score behavior, not launch or transfer
optimization. A one-window physical boundary exposes the first differing
window and keeps the CPU oracle authoritative.

## D-026 — M4 resets state per window and does not claim persistence

**Status:** Accepted for M4

Every M4 score window starts from an explicit all-UNKNOWN 64-byte state. The
device keeps current/next recurrent state local to one dispatch and returns the
result; the host does not reuse device state across windows or candidates.
Topology, LUT, input rows, and targets are retransferred for each operation.
This is deliberately correctness-first and records `persistent_buffers=false`;
buffer reuse and device-resident batching are M5 work.

Reason: implicit state or stale LUT/topology would make exact differential
failures difficult to localize. The chosen behavior is conservative and does
not imply a performance result.

## D-027 — M4 candidate isolation is sequential and CPU-authoritative

**Status:** Accepted for M4

The M4 acceptance driver runs two independent candidate lifecycles in order.
Each lifecycle materializes its own deterministic random material, performs
the complete initial-plus-100-mutation/101-score-call path, verifies every NPU
score against the CPU oracle, and checks the final LUT/current/best state
against a standalone CPU candidate result. No candidate result is accepted
from NPU-only output. This demonstrates reset, ordering, and contamination
isolation without introducing M5 batching or four-column work.

## D-028 — M5 batches complete independent candidate/window pairs

**Status:** Accepted and verified for M5

The first optimization boundary is one complete independent M4 window per
batch item. The item owns its reset state, LUT, topology, input sequence,
targets, and result slot. Candidate mutation, accept/rollback, full-score
reduction, and canonical CPU verification remain host responsibilities.

Reason: a candidate's recurrent ticks are sequential, while complete windows
and candidate/window pairs are independent. Keeping each item as a complete
M4 operation preserves the already verified state-reset, timeout, target-index,
and mismatch boundary and avoids unmeasured cross-column synchronization.

## D-029 — M5 uses explicit lane artifacts and persistent host BOs

**Status:** Accepted and verified for M5

M5 artifacts are generated for fixed `(batch_size, column_count)` pairs. Each
lane is a separate worker explicitly placed on one physical AIE2 column and
receives a contiguous item range; batch order is not reconstructed after the
device. The XRT instruction/input/output BOs are allocated once and reused,
but all active input and output bytes are rewritten/synchronized for every
dispatch. The generated DMA tap covers all `items_per_lane` records; a
single-record tap was rejected after the first multi-item physical test left
the second output at the sentinel. Task/topology/LUT residency inside AIE
local memory is deferred because mutation and task changes must remain visibly
coherent at the M5 contract boundary. The accepted matrix covers batch sizes
1, 2, 4, 8, and 16 across one, two, and four columns with exact results.

Reason: fixed variants make compiler placement, buffer sizing, DMA resources,
and transfer accounting auditable. Reused BO allocation is safe with explicit
sentinel and full-arena rewrites; hidden device context reuse would be a new
correctness risk and is not claimed.

## D-030 — Measure M4 and M5 in separate XRT context lifetimes

**Status:** Accepted and verified for M5

The M5 measurement runner executes the identical logical workload through the
M4 reference first, records its raw timing and transfer counters, destroys the
M4 runtime, and then creates the M5 runtime. On this host, constructing both
hardware contexts concurrently fails with `DRM_IOCTL_AMDXDNA_CREATE_HWCTX`
`err=-19` (`No such device`); this is treated as a runtime limitation rather
than hidden fallback. The two paths remain explicitly separate and use the
same warm-up, repeat, CPU verification, and 16-item corpus rules.

Reason: separate context lifetimes preserve truthful physical evidence while
keeping the comparison workload and measurement method identical.

## Historical decisions retained

The original bootstrap decisions remain valid:

- correctness before performance;
- no silent CPU fallback;
- milestone completion requires evidence;
- mining is only for hardware and endpoints the operator is authorized to use;
- profitability is outside core correctness and benchmark claims.

## D-031 — M6 direct-node protocol is a clean-room CPU boundary

**Status:** Accepted; offline-tested and public read-only system-info
interoperability verified; authorized submission pending

M6 implements the direct-node frame, 128-byte system-info schema, work-context
freshness, exact CPU/NPU submission gate, deterministic broadcast layout, and
bounded TCP/reconnect boundary under `src/qubic/direct_node.*`. It does not
copy Qubic core or Qiner source and does not alter the M5 compute contract.
Networking, task identity, algorithm selection, threshold policy, nonce policy,
and submission authority remain CPU-owned.

Reason: direct-node integration is required before any optional pool adapter,
but the accelerator must remain a deterministic backend behind a correctness
gate. Keeping the finite protocol boundary separate makes stale/mismatch no-
send tests and future provider review auditable.

## D-032 — Production crypto and live submission require explicit injection and opt-in

**Status:** Accepted; blocks live submission until satisfied

The direct-node solution path requires an injected `CryptoProvider` for the
K12-derived gamma/message type and the 64-byte identity signature. The
repository's `UnavailableCryptoProvider` fails closed; the deterministic test
provider is non-cryptographic and may only exercise offline serialization and
mock transport. Runtime submission also requires an explicit opt-in flag in
addition to configured signing material. Secrets are read only from explicitly
named runtime environment variables, are never included in summaries, and are
cleared on `SigningSecret` destruction.

Reason: Qubic's upstream crypto implementation is not reusable under the
project's clean-room/license boundary. Fabricating a signer, inferring a
secret, or treating mock bytes as authenticated network evidence would violate
the correctness and security gates.

## D-033 — M6 selects an optional pinned K12/FourQ provider

**Status:** Accepted for the production-crypto gate; live submission remains
blocked by endpoint and signing-material authorization

The M6 provider implementation is `K12FourQCryptoProvider` in
`src/qubic/production_crypto.*`. It is built only when
`XDNA_ENABLE_PRODUCTION_CRYPTO=ON` and fetches Microsoft FourQlib at commit
`1031567f23278e1135b35cc04e5d74c2ac88c029` (MIT) plus the XKCP K12 repository at
commit `f95b0b73e29fe75fe99fbbb24c8000d9fcf0b40e` (selected source files under
the CC0/public-domain dedication, with the included Brian Gladman endian
notice). FourQlib's documented hash hook is bound to KT128; its bundled
SHA-512 implementation and all Qubic/Qiner crypto source are excluded.

`SigningSecret` is explicitly the 32-byte Qubic signing subseed. The provider
derives the FourQ scalar with K12, checks that the configured public key is the
matching compressed key, and then supplies K12-backed SchnorrQ signing and
compressed ECDH. RFC 9861 K12 vectors, the first synthetic Qubic SchnorrQ
vector, and fixed synthetic public-key/shared-key/gamma vectors are required
by `qubic_crypto_tests` before this provider is considered usable.

Reason: an independently licensed provider is now available without copying
the Anti-Military upstream implementation, while the opt-in build and
injected interface preserve the existing default no-send behavior. This does
not authorize a live endpoint, real user secret, or automatic provider
selection; those remain separate M6 gates.

## D-034 — Complete the public read-only handshake, never invent submission identity

**Status:** Accepted; live system-info PASS, submission remains blocked

The official direct-node listener sends an unsolicited type-0
`EXCHANGE_PUBLIC_PEERS` frame on connection. The adapter must send its own
16-byte zero-peer handshake with a nonzero dejavu before sending request 46,
then ignore ordinary network frames until response 47 arrives. This behavior
was verified against `corenet.qubic.li:21841` on 2026-08-09 and reverified on
2026-08-10; it is covered by the offline mock harness.

The system-info response has no task bytes or destination computor public key.
Current core submission policy also requires a nonzero authorized source
identity satisfying the computor or dissemination-balance rule. Therefore the
project will not generate an ephemeral signer, guess a destination, or send a
candidate without an authorized identity, a current task-compatible candidate,
and the required CPU/NPU evidence. The live gate stays IN PROGRESS until those
external prerequisites exist; this does not authorize M7 or Qatum work.

## D-035 — Use the current computor protocol and pinned core task as external inputs

**Status:** Accepted; destination/task sources verified, authorized submission
still blocked

The current computor destination is obtained from the public node protocol,
not from a hard-coded key: send `REQUEST_COMPUTORS` (type 11) after the normal
peer-exchange handshake and accept a strict `BROADCAST_COMPUTORS` (type 2)
response containing the current epoch, 676 nonzero public keys, and its
signature. A 2026-08-10 read-only request to `corenet.qubic.li:21841` returned
epoch 225 and a 676-key list whose sanitized key-list digest is recorded in
`docs/evidence/m6-direct-node.json`. The helper
`scripts/run-m6-live-computors.sh` is bounded and never sends a solution.

The production BPP9000 task remains the official `data/bpp9000.task` file at
the revalidated core revision, with the core-pinned topology/data hashes and
44,744-byte layout. SystemInfo does not carry task bytes, so the task is an
external hash-verified input; synthetic or production-shaped fixtures cannot
be substituted. No task bytes are copied into this repository.

Reason: destination and task acquisition are now reduced to authoritative
public/current sources, but the core's source authorization still requires
either a current computor secret or a user-owned spectrum identity with at
least `1000000000` energy. Creating/funding such an identity would require
external credentials or user funds, so M6 must remain IN PROGRESS and no
submission may be attempted.

## D-036 — Local identity secrets are generated and erased only through a narrow file boundary

**Status:** Accepted; no persistent operator identity was created by this checkpoint

`m6_identity_tool` uses the Linux CSPRNG, writes the 32-byte signing subseed
as 64 hexadecimal characters plus an optional newline to an explicit 0600
regular file owned by the current user, and expects its directory to be 0700.
It refuses implicit overwrite, does not print the secret, and provides an
explicit overwrite/erase path. The public identity encoding is implemented
clean-room from its observable base-26/K12 checksum format and is tested by
round trips plus the pinned Arbitrator identity. Temporary test identities are
erased before the test exits; no secret is stored in Git, evidence, or chat.

Reason: environment variables and pasted credentials are too easy to log or
misroute. A small local-only boundary makes the authorization workflow
auditable while preserving operator control of any real funds or computor
credential.

## D-037 — Authorization is a read-only official-state gate before candidate search

**Status:** Accepted; authorization and live submission remain IN PROGRESS

`m6_authorization_check` must query SystemInfo, the current signed computor
list, and the source entity before it can print `AUTHORIZED`. It verifies the
current epoch, the 676-key list signature with the pinned Arbitrator identity,
and the official current-computor or spectrum-energy rule. The first verified
current computor key is the only deterministic destination selection; no key
is hard-coded. A transport/protocol/incomplete-state failure prints
`CHECK_UNAVAILABLE`; only a complete valid state can print `NOT_AUTHORIZED`.
The check never starts M5 or writes a network frame.

Reason: a public list observation is not sufficient authorization, and a
successful TCP write is not submission acceptance. State must be cryptographically
and semantically checked before any future candidate runner is allowed to
operate.

## D-038 — Pinned task bytes are cached externally and verified before use

**Status:** Accepted; candidate orchestration remains a later M6 task

`scripts/fetch-m6-bpp9000-task.sh` fetches only the exact core revision
`a83f935406cd006b5b1a94971139e74d410ecb6d`, rejects a mismatched existing
cache unless `--refresh` is explicit, and checks the 44,744-byte file against
SHA-256 `0c5e9e42c6d86c320af62f4125ca85b2446f2b098893fd6521bcf66c22f7f00a`.
`m6_task_verify` then validates the production task using the selected K12
provider. The task is never committed to this repository. The final runner
stops before search because the production random2/candidate orchestration
seam is not yet implemented; it does not fabricate counts or scores.

## D-039 — Bounded official-DNS rotation for read-only query availability

**Status:** Accepted; read-only authorization path verified, submission still disabled

The official `corenet.qubic.li:21841` hostname is the only configured public
endpoint. Because it resolves to multiple public IPv4 direct-node targets and
some targets can accept a TCP connection while returning only asynchronous
traffic during the bounded window, `TcpConnectionFactory` rotates the starting
address across the current `getaddrinfo` answer set for each fresh connection.
Read-only requests use at most eight attempts within the existing 15,000-ms
absolute deadline; connect/read/write timeouts remain 3,000 ms and ignored
byte/frame ceilings remain terminal. No IP is hard-coded, no random
third-party peer is selected, and response validation is unchanged.

The opt-in diagnostics executable records safe request/response metadata and
aggregate ignored message-type counts. A complete live
SystemInfo → signed Computors → Entity(source) check now returns
`NOT_AUTHORIZED` because the preserved local identity is neither a current
computor nor funded to the exact `1000000000` entity-energy threshold. This
decision is trusted read-only state; it does not authorize funding, candidate
search, submission, M7, or Qatum work.

Reason: the observed failure was endpoint target/load nondeterminism, not a
correlation or handshake defect. Increasing the deadline or weakening
same-type dejavu checks would hide the cause and reduce correctness.

## D-040 — Testnet requires an official raw endpoint and a separate trust domain

**Status:** Accepted; public raw testnet endpoint unavailable at the current
preflight

An HTTPS testnet RPC does not satisfy M6 direct-node interoperability. A
testnet endpoint counts only when a current Qubic-controlled source identifies
the raw node, or Qubic supplies a dedicated node explicitly, and the bounded
client completes the existing handshake/SystemInfo path. A historical
official IP that merely accepts TCP but resets before SystemInfo is not a
verified endpoint and must not be promoted into configuration.

Testnet configuration, identities, Arbitrator policy, task inputs, caches, and
submission opt-in must be isolated from mainnet. `XDNA_QUBIC_NETWORK=testnet`
will be required by any future testnet command, but an environment label alone
is not sufficient: the runtime must enforce network-specific defaults and
must refuse cross-network secret paths. Identity creation and candidate work
remain forbidden until SystemInfo, signed Computors, and Entity all pass in
read-only mode.

Current Qubic docs at `qubic/docs`
`236365d69ffb8819e9b621e0bc40006175cb1a78` expose only
`https://testnet-rpc.qubic.org`. Official Core Lite
`df31a9b0dff195b7b4956fe0601ce83baafea9ef` is the accepted source-pinned
local alternative, but a local testnet is separate from public endpoint proof
and requires its own resource/wire-compatibility checkpoint before launch.

Reason: this prevents RPC success, stale hackathon infrastructure, a local
simulation, or a cross-network identity accident from being mislabeled as the
required direct-node testnet submission preflight.

## D-042 — Pearl P1 raw versus quantized value boundary

**Status:** Accepted for Pearl P1

The current raw mining matrix contract accepts signed int8 values `[-64,63]`;
the current quantizer uses `max_val=63` and therefore produces `[-63,63]`.
P1 does not reinterpret the whitepaper's `+64` as a raw accepted value. It
uses fp32 `max_abs/63` scales, no zero point, ties-to-even rounding, and
explicit clamping. The two boundaries are distinct and are represented in the
canonical corpus.

Reason: P0 observed both source-level conventions. Treating them as one range
would make a CPU/NPU comparison appear correct while changing the pinned
mining path.

## D-043 — Pearl P1 checked arithmetic and explicit proof envelope

**Status:** Accepted for Pearl P1

All signed products widen to int64 and reject an int32 result outside the
representable range. P1 does not choose wrapping or saturation without an
authoritative rule. The P1 `PlainProof` uses explicit fixed-width little-endian
fields and stops before CPU/Rust ZK generation; it does not adopt opaque
bincode or create a certificate.

Reason: P0 did not establish overflow behavior, and the requested P1 boundary
needs a stable independently testable object before any proof adapter or NPU
work.
# Pearl one-shot decisions (2026-08-11)

- The P2 XDNA artifact uses the project-owned AIE2 kernel plus canonical IRON
  A/B/C DMA lane transforms. The first direct row-major stream passed simple
  vectors but failed sparse/random cases; it was rejected and replaced after
  exact differential reduction. This is a correctness decision, not a
  performance shortcut.
- P3/P5 keep noise generation, correction, transcript, keyed BLAKE3, target
  comparison, openings, and PlainProof validation on the CPU. The NPU is used
  only for repeated signed-int8 GEMM and every result is compared before a
  candidate is built.
- The pinned historical runtime pin remains unchanged. The host observed
  `amdxdna 7.2.0-0.rc7.452.vanilla.fc45.x86_64` while the older recorded M2
  pin is rc5; evidence records the mismatch as observed rather than silently
  changing the pin.
- The official Pearl useful-work/inference provider and unclear-license
  gateway/prover hot components are external boundaries. This repository
  implements clean-room transport and PlainProof, but never fabricates live
  tensors or copies restricted source.
- P8 selected four columns and batch eight from the fixed physical sweep by
  measured end-to-end dispatch throughput; the choice is not inferred from
  the column count.

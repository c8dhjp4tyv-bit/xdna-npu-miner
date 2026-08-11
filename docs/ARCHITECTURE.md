# Architecture

## Pearl track boundary

Pearl (PRL) is the active implementation track documented in
[`docs/pearl/ARCHITECTURE.md`](pearl/ARCHITECTURE.md). It does not retarget or
rewrite the Qubic architecture below. P1 now contains the clean-room CPU
oracle under `src/pearl/`; it uses no Qubic types or Pearl hot-component source.
Pearl protocol, matrix, proof, network, and license decisions remain
independent. P2-P11 were delivered in one continuous shot with separate
gates. The Pearl boundary keeps networking, canonical verification, ZK proof
generation, and submission on the CPU, with XDNA1 limited to explicitly
verified compute buffers. P7 remains externally blocked.

## M0 architecture status

This is the standalone architecture for current Qubic BPP9000 on AMD Hawk
Point XDNA1/AIE2. It is based on the current source revisions recorded in
`docs/UPSTREAM.md`, especially `qubic/core`
`v1.301.3` (`a83f935406cd006b5b1a94971139e74d410ecb6d`) and Qiner
`v1.302.3` (`11fb18a6f4944bb55fe103d3f263cb5d31e00200`).

M0 is **COMPLETE**. The direct-node compute and protocol boundary is
sufficiently specified for M1. Official Qubic sources conflict about Qatum's
status, but that uncertainty does not block this project: direct-node mining
is canonical, M1 through M5 have zero Qatum/pool dependency, and M6 must
implement and validate direct-node integration first. Qatum/pool support is an
optional, version-gated adapter after direct-node support works; it must not be
implemented until an authoritative, sufficiently complete wire specification
or implementation is pinned and independently reviewed.

No NPU kernel is being implemented in M0.

M1 added the CPU reference layer described below. M2 provides a standalone
XRT/IRON runtime foundation and one-column deterministic hardware smoke. M3
added the first BPP9000 XDNA compute boundary: one isolated K1 recurrent LUT
tick. M4 composed that primitive into a one-column repeated-tick/window score
path with exact CPU verification. M5 adds a fixed-width independent-window
batch path and verified one-, two-, and four-column artifacts. Mutation
control remains CPU-owned. M6 now provides the clean-room direct-node framing,
system-info/work-context, submission-gate, solution-serialization, bounded
transport, and mock integration boundary around that backend. An optional
K12/FourQ provider is now selected and KAT-verified, but live interoperability
and the continuous M7 supervisor remain out of scope.

## System boundary

```text
+-----------------------------+
| Qubic node                   |
| current seed, tick, threshold|
+--------------+--------------+
               |
               | TCP: framed, signed/encrypted broadcast messages
               | system-info response for epoch/seed/threshold
               v
+-----------------------------+
| CPU network / epoch manager |
| task file validation        |
| stale-work and reconnect    |
+--------------+--------------+
               |
               v
+-----------------------------+
| CPU candidate/control       |
| nonce policy               |
| random2/K12 orchestration   |
| mutation, accept/rollback   |
+--------------+--------------+
               |
               | one physical XRT dispatch per M4 operation
               v
+------------------------------------------------+
| XDNA1/AIE2 compute backend                     |
| repeated-tick/one-window recurrent LUT score  |
| state local to one dispatch; batching deferred  |
+----------------------+-------------------------+
                       |
                       v
+-----------------------------+
| CPU canonical verification  |
| exact score/threshold gate  |
+--------------+--------------+
               |
               v
+-----------------------------+
| CPU signed solution submit  |
| future versioned pool share |
+-----------------------------+
```

The node does not currently send a canonical BPP9000 “job blob” in the direct
solution path. The CPU combines the hash-verified local task with current
system information and locally generated candidate nonces.

## Responsibilities and module boundaries

The M1 implementation uses these standalone source boundaries:

| Area | CPU responsibility | NPU responsibility |
|---|---|---|
| `src/bpp9000/task` | Parse/serialize the 96-byte header/topology/data; validate dimensions, trits, hashes | None |
| `qubic/epoch` | Request/parse system info; track epoch, tick, 32-byte seed, threshold; invalidate stale work | None |
| `qubic/network` | 8-byte frame, signatures, gamma/encryption, reconnect, submission | None |
| `src/bpp9000/reference` | Scalar BPP9000 oracle, LUT, recurrent state, window/full scoring, candidate control | None |
| `src/bpp9000/random` | Seed-aware injectable root/mutation draw boundary and deterministic fixture source | None |
| `xdna/runtime` | Device/version discovery, XRT buffers, dispatch and evidence | Execute selected deterministic buffers |
| `xdna/k1` and `xdna/runtime` | Pack one K1 tick, synchronize, validate output shape/status | One isolated recurrent LUT/tick primitive |
| `xdna/m4` and `xdna/m4_score` | Pack repeated ticks/window inputs, dispatch one operation, compare every result with the CPU oracle | Device-local repeated ticks and one-window score/status |
| `xdna/m5` and `xdna/runtime` | CPU pack fixed batches, preserve candidate/window indices, rewrite reusable BOs, verify every item | Independent M4 window operations on explicit AIE2 lanes |
| `src/qubic/direct_node` | Frame/system-info parsing, WorkContext freshness, CPU/NPU submission gate, crypto injection, solution serialization, bounded TCP/reconnect | None |
| verification | Recompute candidate with the CPU oracle before submission | Never authorizes a share by itself |
| benchmark | Timestamp, workload identity, transfer/telemetry accounting | Report actual dispatch/kernel timing |

A future pool adapter must sit beside the direct-node network adapter. It must
not alter the CPU/NPU compute contract. Pool-specific proprietary protocols
are adapters at this boundary, not part of the mining/scoring core.

## M6 direct-node boundary

`src/qubic/direct_node.*` owns only the finite protocol boundary. It parses the
8-byte little-endian request/response frame, decodes the exact 128-byte
`RespondSystemInfo` payload, attaches the locally hash-validated task identity
and BPP9000 algorithm to a `WorkContext`, and rejects older epoch/tick or
changed-seed work. A candidate is eligible only when its nonce, seed, task,
algorithm, finite full score, threshold, and exact CPU/NPU evidence all agree.

The M5 backend remains the compute provider: M6 consumes CPU-recomputed full
score evidence and never lets an NPU-only result authorize a message. Solution
serialization preserves the source/destination/gamming nonce prefix, the
68-byte encrypted seed/nonce/score payload, and the 64-byte signature. The
`CryptoProvider` is deliberately injected because Qubic's upstream crypto
implementation is not reusable under this project's clean-room/license rule.
`UnavailableCryptoProvider` still fails closed. The opt-in
`K12FourQCryptoProvider` uses the pinned external sources recorded in
`docs/UPSTREAM.md`, binds FourQlib's hash hook to KT128, treats the configured
secret as a 32-byte signing subseed, and checks the derived public key before
signing or ECDH. Its RFC/Qubic/fixed-byte KATs are in
`tests/qubic_crypto_tests.cpp`; the provider is not automatically selected for
runtime submission. Network submission also requires an explicit runtime
opt-in, an authorized endpoint and signing secret, bounded connect/read/write
timeouts, and bounded reconnect attempts.

Read-only queries retain one absolute 15,000-ms deadline across reconnects and
use at most eight attempts. `TcpConnectionFactory` rotates the starting
address across the current IPv4 answers for the official `corenet.qubic.li`
hostname on each fresh connection; it does not embed or discover arbitrary
third-party peers. This is a bounded availability policy for the observed
official DNS fan-out, not a deadline extension. Optional
`ReadOnlyRequestDiagnostics` reports only request/response metadata, elapsed
time, resource totals, and aggregate ignored message-type counts; payloads and
signing material are never logged.

The live direct-node connection begins with a type-0 `EXCHANGE_PUBLIC_PEERS`
frame containing 16 zero bytes and a nonzero per-connection dejavu, followed
by the type-46 system-info request. A node can send the peer-exchange frame
and ordinary broadcast traffic asynchronously, so the bounded reader ignores
those network frames and accepts only the desired response type plus exact
request-dejavu correlation (type 47 for SystemInfo, type 2 for Computors, and
type 32 for Entity). A same-type wrong-dejavu frame fails closed before the
asynchronous allowlist is consulted. The
read-only executable is `qubic_live_probe`, driven by
`scripts/run-m6-live-system-info.sh`; it keeps the safe localhost default in
`RuntimeConfig` unchanged and sets the official public endpoint explicitly.
The response contains runtime epoch/tick/seed/threshold but no task bytes or
destination computor key. A separate bounded read-only
`scripts/run-m6-live-computors.sh` request uses type 11
`REQUEST_COMPUTORS` and validates the exact type 2 `BROADCAST_COMPUTORS`
shape, current epoch, and nonzero public keys; it does not claim Arbitrator
signature verification and never submits a solution. The probe therefore
constructs a context from the recorded task identity, labels full live task
compatibility as unproven, and does not submit. The production task remains
the pinned, hash-verified upstream core input rather than a SystemInfo field.

### M6 local identity and authorization boundary

`m6_identity_tool` is the only supported local identity workflow. `generate`
uses Linux `getrandom()` for a fresh 32-byte signing subseed and stores only
lowercase hexadecimal text in an explicitly named owner-only 0600 file under a
0700 directory. `show` derives and prints only the public key and clean-room
60-character identity; `erase` overwrites, synchronizes, closes, and unlinks
the explicit file. The secret is never placed in environment variables,
evidence JSON, logs, command output, or the repository. The suggested paths
are ignored by Git, but an operator must still inspect the path before any
authorized use.

`m6_authorization_check` is read-only. It queries SystemInfo, the current
type-11/type-2 computor list, and the type-31/type-32 entity record; verifies
the current epoch, all 676 nonzero keys, and the 64-byte computor signature
with the pinned Arbitrator public identity; then applies the official
`incomingAmount - outgoingAmount >= 1000000000` rule. A source is authorized
only when it is a verified current computor or a verified funded spectrum
entity. The first key in the verified current list is the deterministic
destination selection; no destination key is hard-coded. The entity response
is exactly 840 bytes and the computor response is exactly 21,698 bytes.

The pinned task is fetched only through
`scripts/fetch-m6-bpp9000-task.sh`, which rejects a mismatched existing cache
unless `--refresh` is explicit and verifies the exact size/SHA-256 before an
atomic move. `m6_task_verify` then parses the cached bytes with the production
K12 digest provider. `scripts/run-m6-final-live-submit.sh` requires explicit
opt-in, runs the production KAT, performs the authorization check before any
candidate search, and currently stops with zero candidate/score/frame counts
because a production random2/candidate-orchestration runner over the pinned
task is not yet wired. It never fabricates a WorkContext, NPU score, CPU
verification, signature, retry, or submission.

### M6 production crypto gate

The default CMake configuration does not fetch or link crypto dependencies.
`-DXDNA_ENABLE_PRODUCTION_CRYPTO=ON` enables the pinned FourQlib/K12 build and
registers the seven-test CTest suite, including exact K12, public-key, ECDH,
SchnorrQ signature, gamming-key, and 68-byte gamma-stream vectors. These tests
use synthetic values only. Passing them establishes provider correctness for
the exercised primitives; it does not establish node interoperability or
authorize a live submission.

### M6 testnet isolation boundary

Testnet is a separate trust domain, not an endpoint override on the mainnet
identity workflow. Any future testnet path must require an explicit network
selection, a network-specific endpoint and Arbitrator/task policy, and
separate secret/cache paths. A mainnet identity file must never be a fallback
for testnet, and a testnet identity must never be accepted by a mainnet
command. Read-only discovery must run with signing material absent and live
submission disabled.

The 2026-08-10 preflight found no currently verifiable official public raw
testnet endpoint. Current official docs expose only an HTTPS RPC service;
historical official dedicated/shared hosts refused, timed out, or reset the
connection before SystemInfo. Therefore no testnet-specific runtime or
identity configuration is installed yet, and no Computors, Entity, candidate,
or submission stage is allowed to run. Official Core Lite at
`df31a9b0dff195b7b4956fe0601ce83baafea9ef` is the source-pinned local
alternative on port 31841, but provisioning it is a distinct resource and
wire-compatibility gate. A local simulation must be labeled separately from
public testnet interoperability.

## M2 runtime foundation

The M2 runtime is a standalone host-side C++20/XRT layer under `src/xdna/`.
`device` opens the selected XRT device and requires corroborating `xrt-smi`
identity before reporting `SUPPORTED_XDNA1`; environment variables alone never
grant support. `buffers` defines the smoke contract as contiguous `int32[32]`,
128 bytes, 4-byte alignment. `runtime` loads an Iron-generated XCLBIN and
instruction stream, creates an XRT hardware context and `MLIR_AIE` kernel,
allocates reusable instruction/input/output BOs, and performs explicit H2D and
D2H synchronization. A dispatch is successful only after
`ERT_CMD_STATE_COMPLETED` and an exact CPU comparison.

The artifact generated by `src/xdna/smoke_program.py` uses one AIE2 column and
computes `out[i] = 3 * in[i] + 7`. It is intentionally unrelated to BPP9000;
there is no LUT, recurrent state, mutation, scoring, network, signing, or
production crypto in the smoke program. M2 verifies allocation, visibility,
device arithmetic, completion, reuse, and fail-closed errors, not performance
or four-column utilization. Evidence is written to
`docs/evidence/m2-xdna-smoke.json`; project-owned stack pins are in
`runtime-pins.json`.

## M3 K1 isolated recurrent tick

M3 is **COMPLETE** for the first isolated deterministic BPP9000 primitive. The
M1 scalar `recurrent_tick` is the trusted CPU oracle. The physical runtime
path in `src/xdna/runtime.cpp` only validates, packs, synchronizes, dispatches,
waits for `ERT_CMD_STATE_COMPLETED`, reads back, and validates the device
output; it never computes the expected state or silently falls back to CPU.

The logical K1 input/output contract is:

```text
previous_state:  uint8[64]          trits 0, 1, 2; 2 = UNKNOWN
lut:             uint8[46][32]      entries 0..26 logical; 27..31 padding
neighbors:       uint32[64][3]      serialized topology rows
updated_neurons: uint32[46]         ascending non-input rows
next_state:      uint8[64]          logical output prefix
```

For each updated row, the device reads all three values from `previous_state`,
forms `first + 3*second + 9*third`, reads `lut[row][index]`, and writes that
row of `next_state`. The 18 neurons absent from `updated_neurons` are copied
unchanged. This is simultaneous double-buffer semantics, not in-place
recurrent mutation.

The physical one-column AIE2 artifact uses a single 2,528-byte aligned input
arena and a 96-byte output BO:

| Region | Device offset | Size | Logical meaning |
|---|---:|---:|---|
| state | 0 | 96 bytes | 64-byte state plus unused padding |
| LUT | 96 | 1,472 bytes | 46 rows × 32 bytes |
| neighbors | 1,568 | 768 bytes | 64 rows × 3 `uint32_t` |
| updated rows | 2,336 | 192 bytes | 46 logical words plus 2 pad words |
| output | separate BO | 96 bytes | 64-byte next-state prefix |

The combined arena was chosen because the one-column compiler/AIE shim
rejected the initial independent-stream design for exceeding its DMA channel
budget. This is a device-placement constraint only; the host logical
schema remains explicit and is tested through pack/unpack and padding-isolation
cases. Each isolated tick performs two H2D synchronizations (input arena and
sentinel output) and one D2H synchronization.

M3 verified one column only. State is not retained on the device across host
dispatches, and no full-window score, mutation loop, candidate batch, or
four-column performance experiment is part of this milestone.

## M4 full CPU/NPU score correctness path

M4 is **COMPLETE** for the exact one-column score boundary. The clean-room
Iron/MLIR-AIE artifact has three auditable modes: one K1 tick, repeated ticks
with explicit input rows, and one complete signal-paced score window. The
device never computes a CPU oracle or authorizes a candidate; the host runs
the independent M1 scalar window, compares score/status/ticks/feed/predicted/
expected fields exactly, and only then reduces windows into a score result.

The fixed device contract is:

| Region | Device offset | Size | Logical meaning |
|---|---:|---:|---|
| control | 0 | 64 bytes | magic, mode, counts, roles |
| state | 64 | 96 bytes | 64 logical trits plus padding |
| LUT | 160 | 1,472 bytes | 46 rows × 32 bytes; 27 logical entries |
| neighbors | 1,632 | 768 bytes | 64 × 3 `uint32_t` |
| updated rows | 2,400 | 192 bytes | 46 logical rows plus device padding |
| input roles | 2,592 | 72 bytes | 18 input neuron indices |
| input sequence | 2,688 | 12,096 bytes | up to 672 rows × 18 trits |
| targets | 14,784 | 673 bytes | up to 673 output trits |
| total input | — | 15,488 bytes | one aligned operation arena |
| output | separate BO | 128 bytes | state, score/status, counters, diagnostics |

Every window starts from a host-created all-UNKNOWN 64-byte state. State is
double-buffered inside one device dispatch and is discarded after the result;
there is no implicit state between windows or candidates. Topology, LUT, input
rows, and targets are retransferred for each dispatch in M4. This is an
explicit correctness-first residency choice, not a transfer or throughput
claim; persistent/reused buffers are deferred to M5.

The host performs one physical XRT dispatch per repeated-tick result or score
window. A full production-shaped score therefore executes all 8,088 windows,
with CPU/NPU comparison at every window and host-side exact score reduction.
The candidate path keeps random materialization, mutation, accept/rollback,
and final CPU candidate comparison on the host. Two sequential 101-score-call
candidate paths verify reset and ordering isolation. M4 remains one-column,
does not implement networking or crypto, and records no performance claim.

## M5 batched independent-window path

M5 selects the independent candidate/window pair as its work unit. One batch
item is one complete M4 `WindowScore` operation: it starts from an explicit
all-UNKNOWN state, carries one candidate's LUT and task/topology view, feeds
one window, and returns exactly one score/status/result record. Candidate
mutation, accept/rollback, full-score reduction, and CPU verification remain
outside the device batch boundary. This preserves the M4 reset semantics and
avoids synchronizing one recurrent candidate across columns.

The M5 host schema is fixed-width and row-major:

```text
batch item i
  candidate_index: uint32
  window_index:    uint64
  input_offset:    i * 15488 bytes
  output_offset:   i * 128 bytes
  input_item_stride: 15488 bytes
  output_item_stride: 128 bytes
  state/LUT/topology/input/target offsets: M4 offsets relative to input_offset
  score/status/ticks/feed/predicted/expected/error: output offsets relative to output_offset
```

The host never reorders results: output slot `i` is decoded with metadata
`(candidate_index, window_index)` from input item `i`. The per-item status is
the settled/timeout field and the per-item error word is zero only for a
normal kernel result. A device/runtime failure rejects the whole batch and is
not converted into per-item success.

For the first M5 implementation, each lane receives a complete independent
M4 arena. This deliberately repeats task/topology/LUT bytes when windows are
batched, making the correctness and transfer tradeoff measurable without
introducing an unverified device-resident context. XRT input/output BOs and
the instruction BO are allocated once per runtime and reused; every dispatch
rewrites the entire active input arena and sentinel-initializes the entire
output arena before synchronization, so stale state/output cannot satisfy a
match. Buffer classes are:

| Buffer/data | M5 class | Rule |
|---|---|---|
| artifact/instructions and compiled lane placement | IMMUTABLE | fixed for one runtime/artifact |
| task/topology and LUT bytes inside an item | PER-CANDIDATE / PER-WINDOW copy | repeated per item until a safe device context path is measured |
| initial state, input sequence, targets | PER-WINDOW | reset and replaced for every logical item |
| mutation records and candidate snapshots | PER-MUTATION | CPU only; never stale device state |
| output/status arena and control metadata | PER-DISPATCH | sentinel-initialized and read back for every batch |

The artifact variants are compiled for one, two, or four columns and a fixed
batch size. Worker `lane j` is explicitly placed on the generated artifact's
column lane and processes the contiguous item range assigned to that lane. A
batch size must be divisible by the selected column count; this makes the
mapping and ordering auditable. The runtime tap transfers all
`items_per_lane` records for each lane, not just the first record. Generated
AIE placement plus a unique CPU-verified result for every lane are required
evidence for a claimed active-column configuration. Sequential one-column
dispatches are not relabeled as multi-column execution.

M4's one-operation-per-dispatch path remains unchanged and is measured as the
control for the identical window-item corpus. M5 reports raw transfer bytes,
syncs, physical dispatches, host preparation, dispatch/wait, CPU verification,
and repeated wall-time samples. No speedup is inferred unless workload,
verification policy, warm-up, repeats, and raw values are identical.

M5 is **COMPLETE** on the verified Hawk Point host. The reproducible command
`./scripts/run-m5-validation.sh` measured 16 deterministic independent
candidate/window pairs with two warm-ups and five measured repeats per
configuration. All nine configurations below had 80/80 exact measured item
matches, zero mismatches, and zero runtime failures; the full raw records,
artifact hashes, UUIDs, generated partition metadata, and lane evidence are in
`docs/evidence/m5-batching-four-column.json`.

| Path | Batch/columns | Median wall ms (p95) | Measured dispatches | H2D syncs / bytes | D2H syncs / bytes |
|---|---:|---:|---:|---:|---:|
| M4 reference | 1 / 1 | 2.479492 (3.015180) | 80 | 160 / 1,246,800 | 80 / 10,240 |
| M5 | 1 / 1 | 2.655153 (2.836824) | 80 | 160 / 1,249,280 | 80 / 10,240 |
| M5 | 2 / 1 | 1.814122 (2.019287) | 40 | 80 / 1,249,280 | 40 / 10,240 |
| M5 | 4 / 1 | 1.586132 (1.844287) | 20 | 40 / 1,249,280 | 20 / 10,240 |
| M5 | 2 / 2 | 1.966177 (2.239110) | 40 | 80 / 1,249,280 | 40 / 10,240 |
| M5 | 4 / 2 | 1.462820 (1.864036) | 20 | 40 / 1,249,280 | 20 / 10,240 |
| M5 | 8 / 2 | 1.550024 (1.916814) | 10 | 20 / 1,249,280 | 10 / 10,240 |
| M5 | 4 / 4 | 1.405202 (1.514668) | 20 | 40 / 1,249,280 | 20 / 10,240 |
| M5 | 8 / 4 | 1.133271 (1.174929) | 10 | 20 / 1,249,280 | 10 / 10,240 |
| M5 | 16 / 4 | 1.067016 (1.451890) | 5 | 10 / 1,249,280 | 5 / 10,240 |

The selected M5 configuration is batch 16 / four columns because it had the
lowest measured median wall time in this matrix. The one-column M5 control is
slower in this run because its fixed per-item arena includes 31 bytes of
explicit padding; this is recorded as raw timing, not a speedup claim. The
four-column artifacts have generated `npu1_4col` placement with four core
workers on row 2 and partition width 4; each lane received distinct fixture
inputs and returned its own CPU-verified result. Two-column artifacts have
generated `npu1_2col` placement with partition width 2. The selected runtime
reuses instruction/input/output BOs but destroys the M4 comparison context
before creating M5 because this XRT environment does not support both hardware
contexts concurrently.

The M6 compute contract is therefore: submit fixed batches of up to 16
independent pairs to the selected batch-16/four-column artifact, use the
15,488-byte input and 128-byte output strides, rewrite all item input/state/LUT/
topology/sequence/target bytes for every dispatch, preserve slot ordering, and
CPU-recompute every returned window before mutation reduction or submission.
Task/context invalidation is represented by a new packed batch/runtime boundary;
no device-resident candidate or epoch state is assumed.

### M1 CPU API and storage contract

`parse_task(bytes, options)` requires exact magic/version, explicit
little-endian fields, checked block lengths, role/index bounds, valid packed
base-3 bytes, no trailing bytes, and an injected digest provider when hash
metadata is present. `serialize_task` regenerates the canonical byte layout
without relying on native struct padding.

`Lut` stores only logical entries 0..26 in explicit 32-byte rows; padding is
zeroed. `RecurrentState` owns separate current/next byte buffers. Each tick
copies input state to the next buffer and computes every non-input neuron from
the previous buffer using `t0 + 3*t1 + 9*t2`. `score_window` and `score_lut`
return exact uint32 scores or `0xffffffff` on timeout. `score_candidate` keeps
mutation/rollback on the CPU and reports all 101 score calls for the default
100-step search.

M1's `DeterministicFixtureDigest` and `DeterministicFixtureRandom` are test
seams, not production cryptography. Production K12/random2 task integration
must be independently reviewed and supplied behind the same interface.

## Protocol scope boundary

The direct Qubic-node path is the canonical protocol path for this project.
The current core/Qiner direct-node behavior recorded in `docs/UPSTREAM.md` is
sufficient for M0; Qiner remains a behavioral/reference aid and core remains
the consensus and validation authority. M1 through M5 must have zero
dependency on Qatum or any mining pool.

M6 must implement and validate direct-node integration first. Qatum/pool
integration may be added only after the direct-node path works and an
authoritative, sufficiently complete, versioned wire specification or
implementation has been pinned and independently reviewed. Until then, no
Qatum wire behavior may be invented or inferred from a status page, an older
announcement, or a proprietary pool adapter.

## Exact BPP9000 state model

Production values are:

- task inputs `inputs[T][N]`: `uint8_t` trits, values 0, 1, 2;
- task outputs `outputs[T][M]`: `uint8_t` trits, values 0, 1, 2;
- neuron state: `uint8_t[P]`, double-buffered per tick;
- LUT: `uint8_t[U][32]`; only entries 0..26 are logical, stride 32;
- neighbors: `uint32_t[P][K]` in the serialized topology;
- mutation seeds: `uint64_t[S][L]` after random2, padded to 8,064 bytes;
- score: `uint32_t`; valid scores are not `0xffffffff` and are at most
  `numberWindows == 8,088`;
- timeout sentinel: `0xffffffff`;
- random2 pool: approximately `2^32 / 8` bytes, padded to a multiple of
  200 bytes for the 200-byte Keccak state;
- canonical nonce: 32 bytes with `nonce[0] == 1`, `1 <= nonce[1] <= 10`,
  `nonce[2] == 0`.

There are no numeric synapse weights in BPP9000. The wiring is a
`uint32_t[P][K]` neighbor-index table: each of the 64 neurons has three
neighbor indices in `[0, P)`; the source validator requires bounds and role
index uniqueness but does not require neighbor indices to be unique. The LUT
values are the only per-neuron learned values and are trits `0,1,2`.

There is no dot-product accumulation or saturation in the recurrent update.
The base-3 LUT index is an unsigned value in `0..26`; the score is a
`uint32_t` failure count bounded by 8,088, and the tick counter is a
`uint32_t` bounded by 100,000. Core's per-candidate ANN storage is
`alignas(64)` with 32-byte LUT row stride; the packed task header is exactly
96 bytes and must not inherit host struct padding. Any XRT/device alignment
or padded layout must be explicit in a pack/unpack contract.

The update of all 46 non-input neurons in a tick reads the previous state
buffer. Input neurons preserve their externally assigned state for the tick.
This simultaneous-commit rule is part of the CPU/NPU contract; an in-place
update is not equivalent.

## Conceptual operation inventory

Counts below are per epoch, per candidate, or per score as stated. They are
static source-derived counts, not performance measurements.

| Operation | Count | Datatype / traffic | Dependencies and branches | XDNA1 assessment |
|---|---:|---|---|---|
| Load/hash task file | once per task | 44,744 bytes; K12 hashes over topology/data | file and validation branches | LOW; CPU |
| Generate random2 pool | once per epoch | 200-byte Keccak state repeated over ~512 MiB pool | sequential permutation and large writes | LOW; CPU/host memory |
| Root K12/random2 LUT init | once per public key, then 1,728 logical draw bytes (1,792 padded) per candidate path as initialized | 32-byte hash input; byte pool reads | crypto control, candidate setup | LOW; keep CPU unless measured |
| Search K12/random2 | once per candidate; 8,000-byte seed payload, 8,064 padded | 32-byte hash; pseudo-random pool reads | candidate-specific and low reuse | LOW; CPU |
| Initialize ANN state/LUT | once per score call | 64 state bytes; 46*32 LUT bytes | reset/control | LOW |
| One mutation | `L` per step, 100 steps; 100..1,000 mutations | one `uint64_t` seed and one LUT byte update | modulo/index/accept path | LOW as standalone |
| Recurrent tick | 46 LUT evaluations/tick; three neighbor reads and one byte store per updated neuron | `uint8_t` state/LUT; index 0..26 | tick-to-tick state dependency; within-tick parallelism | HIGH candidate, benchmark required |
| Input feed/reset | up to 18 writes/tick/window | task `uint8_t` trits | signal/feeding branch | Fuse with recurrent engine |
| Window score | 8,088 windows/score; one output compare/window | state plus 672*18 input trits/window | variable settle length, timeout branch | MEDIUM when batched |
| Mutation accept/rollback | 100 score comparisons and snapshots | about 1,242 logical LUT bytes/step | serial control and branch | LOW standalone |
| Candidate batch/reduction | `B` independent candidates | candidate LUT/state and `B` scores | independent across candidates | HIGH potential; measure |
| Canonical verification | at least one full score for a found result | same exact scalar semantics | CPU gate before submit | CPU authoritative |

The recurrent tick is dot-product-adjacent only in its regularity; it is not a
dense multiply. It is a byte LUT indirection with three byte reads. The trits
are `0,1,2`, not `-1,0,+1`; multiplication cannot be replaced by ternary
add/subtract arithmetic. There is no floating point and no documented
saturation arithmetic in the score path.

### Data reuse and serial portions

- The 18-input/one-output task is constant across candidates and should be
  resident or reused in host/device buffers.
- The topology is constant; each complete candidate reuses the same neighbor
  map.
- A root LUT generated from the public key can be cached at the CPU boundary,
  but mutation paths are candidate-specific.
- A candidate's recurrent state is sequential across ticks. A single candidate
  cannot be split across columns by time without a synchronization on every
  tick.
- Windows and candidates are independent at their outer boundaries. They are
  the natural sources of batch parallelism.
- Signal settling produces variable work per window. Any fused NPU kernel must
  carry a per-lane active/settled mask or use a bounded fixed loop; either
  choice needs differential tests.
- Mutation accept/rollback and network freshness are CPU control operations.
  Moving them alone to XDNA1 would add transfer and synchronization without a
  known benefit.

## Candidate XDNA1 kernels

The following are hypotheses to benchmark after M1/M2. Shapes use a batch
dimension `B`; `B` is not fixed in M0.

### K1 — recurrent tick, candidate-batched

**Classification: HIGH suitability, UNKNOWN — NEEDS BENCHMARK**

| Field | Specification |
|---|---|
| Inputs | `state[B][P]` `uint8_t`; `lut[B or 1][U][32]` `uint8_t`; `neighbors[U][3]` `uint32_t`; optional `input[B][N]` |
| Outputs | `next_state[B][P]` `uint8_t`; optional settled/signal mask |
| Work | `B*U` LUT evaluations/tick, `3*B*U` neighbor byte reads, `B*U` stores, plus at most `B*N` input writes |
| Vectorization | Pack independent candidates or windows across lanes; keep the three-neighbor index and LUT lookup explicit; do not use signed dot-product assumptions |
| Local memory | Per item 64 state bytes; per candidate 46*32 LUT bytes if private; 552 bytes for 46*3 32-bit neighbor indices if replicated; actual tile allocation must be measured |
| XRT/external buffers | Batch state, LUTs, task inputs, topology, output status; prefer persistent buffers across ticks in a fused dispatch |
| Reuse | Topology/task reused for all candidates; LUT/state reused across ticks of one candidate |
| Synchronization | One dispatch boundary per batch is preferred; per-tick host round trips are unacceptable for a throughput design |
| Four-column mapping | Partition complete candidates/windows across four columns, approximately `B/4` each; do not split one recurrence across columns initially |
| Host transfer risk | High if state is copied after every tick; low enough to investigate if state stays device-resident |
| Correctness risks | Previous-vs-next state buffer, trit indexing, unknown handling, input timing, per-lane masks, LUT stride |
| Likely bottleneck | Byte LUT/local-memory access and control divergence, not arithmetic throughput |

### K2 — fused window-batched scoring

**Classification: MEDIUM suitability, UNKNOWN — NEEDS BENCHMARK**

| Field | Specification |
|---|---|
| Inputs | `inputs[B][W][N]` `uint8_t`; `outputs[B]` or target rows; topology; one LUT per candidate or shared LUT |
| Outputs | `score[B]` `uint32_t`; timeout/status per batch item |
| Work | Up to `B*W` feed iterations and `B*U*ticks` recurrent updates, with 8,088 windows per full score |
| Vectorization | Batch independent windows/candidates; maintain per-lane active, feed counter, tick counter, and score |
| Local memory | 64 state bytes per lane plus LUT/topology tiles; task row tiles should be reused instead of copied per tick |
| XRT/external buffers | Canonical task data, candidate LUTs, output scores/status; persistent task buffer is preferred |
| Reuse | Same topology and task for all lanes; candidate LUT across all windows |
| Synchronization | One score or multi-score dispatch; CPU synchronizes only at score/result boundary |
| Four-column mapping | Windows or candidates can be sharded; candidate sharding is simpler when LUTs differ |
| Host transfer risk | Moderate for one full score; high if each window returns to host |
| Correctness risks | Variable settling divergence, exact window indexing (`t+W`), timeout sentinel, reset-to-unknown |
| Likely bottleneck | Divergent control and state/LUT memory access |

### K3 — full candidate search/fused mutation loop

**Classification: MEDIUM suitability as a fused experiment; LOW as a
standalone kernel; UNKNOWN — NEEDS BENCHMARK**

| Field | Specification |
|---|---|
| Inputs | `lut[B][U][32]`; `mutationSeeds[B][100][10]` `uint64_t`; task/topology |
| Outputs | best `score[B]` and best LUT/nonce metadata |
| Work | Up to 100 mutation steps, each applying `L` entries and invoking a full score |
| Vectorization | Independent candidates across lanes; mutation table update is irregular |
| Local memory | Current/best LUT snapshots (logical 1,242 bytes or padded rows), state and task tiles |
| XRT/external buffers | Candidate seeds/LUTs in, best score/LUT out; large output only if best candidate is returned |
| Reuse | Task/topology and root LUT reused; score state can stay local |
| Synchronization | Must not synchronize host after each mutation; accept/rollback should be inside a fused device program if attempted |
| Four-column mapping | Candidate sharding; do not distribute one mutation path across columns |
| Host transfer risk | Low only if the entire search remains device-resident; otherwise prohibitive |
| Correctness risks | Snapshot/rollback, `r <= current`, `K==0`, nonce-to-seed derivation, exact best-score tie behavior |
| Likely bottleneck | Control, irregular LUT writes, state capacity, and device program complexity |

M1/M3 should start with K1 or a smaller scalar recurrent primitive, not K3.
K3 is not required to establish the architecture.

### K4 — score reduction / output compare

**Classification: LOW as a separate kernel; MEDIUM only when fused into K2**

| Field | Specification |
|---|---|
| Inputs | predicted output trits and target trits, logically `[B][8088]` |
| Outputs | `uint32_t score[B]` and timeout flags |
| Work | up to `B*8088` byte comparisons and additions |
| Vectorization | simple byte compare/reduction |
| Local memory | small reduction state |
| External buffers | unnecessary if K2 already owns predictions |
| Reuse | none beyond one score |
| Synchronization | separate dispatch would add a result transfer boundary |
| Four-column mapping | trivial batch reduction |
| Host transfer risk | dominates the small arithmetic if isolated |
| Correctness risks | `0xffffffff` timeout and score width |
| Likely bottleneck | launch/transfer overhead |

### K5 — random2 pool/K12/hash support

**Classification: LOW suitability; UNKNOWN — NEEDS BENCHMARK if a complete
licensed crypto backend is ever selected**

The pool is approximately 512 MiB and is generated once per epoch by repeated
200-byte Keccak permutations. Per-candidate K12/random2 work has random pool
reads and little reuse. This is not a reason to put network, identity, or
cryptographic signing on the NPU. Keep it on the CPU until measurements and
license review justify anything else.

## Reference and verified XDNA1 environment

The related `hawkpoint-npu-llm` checkout was inspected only for environment
and engineering patterns. Its validated pin file records Fedora x86, device
`RyzenAI-npu1`, AMD XDNA
`7.2.0-0.rc5.260729.fc02acf6.441.vanilla.fc45.x86_64`, firmware `1.5.5.391`,
XRT `2.26.0`, and MLIR-AIE commit
`57d7494e99c214f5f53b328a0ed43a99e759e835`. The observed patterns are
device discovery, explicit XRT buffer synchronization, persistent IRON
programs, four-column selection, `xrt-smi` evidence, and fail-closed hardware
validation. M2 independently verified the current host as `RyzenAI-npu1` /
`aie2`, BDF `0000:06:00.1`, with `/dev/accel/accel0`, firmware `1.5.5.391`,
XRT `2.26.0`, amdxdna/kernel
`7.2.0-0.rc5.260731.8ba098e6.443.vanilla.fc45.x86_64`, MLIR-AIE commit
`57d7494e99c214f5f53b328a0ed43a99e759e835`, `mlir_aie` `1.3.4`, and Peano
`llvm-aie 21.0.0.2026072001+ce8c0f8f`. The related checkout remains an
informational reference and is not a dependency; its old `...441...` kernel
record was not forced onto this host.

## CPU/NPU correctness contract

The boundary is a fixed-width, row-major schema:

```text
CPU input:
  task trits uint8[N/T], topology uint32[P*K],
  LUT uint8[U][32], state uint8[P], candidate metadata
NPU input:
  byte-identical fields plus explicit lengths/strides
NPU output:
  uint32 score, timeout/status, and optional state/prediction diagnostics
CPU check:
  exact field equality, then canonical CPU recomputation before submission
```

No tolerance is permitted for trits, LUT values, state, score, timeout, or
indices. Any future padded layout must have an explicit pack/unpack test and
must not change logical row/stride semantics.

Required contract cases are enumerated in `docs/TESTING.md`. In particular,
“negative values” means rejection tests for invalid signed input: BPP9000 has
no negative trit. The value `2` is UNKNOWN and must never be treated as `-1`.
There is no score saturation/clamp; test the actual bounds and timeout
sentinel instead.

## Failure and fallback policy

The runtime must distinguish:

- task hash/dimension/trit rejection;
- stale or zero mining seed;
- invalid nonce;
- missing XDNA device or incompatible stack;
- compile/load/dispatch/synchronization failure;
- exact CPU/NPU mismatch;
- node rejection, timeout, or reconnect;
- unavailable NPU/power telemetry.

M2 capability status names are `SUPPORTED_XDNA1`, `NO_XDNA_DEVICE`,
`WRONG_XDNA_GENERATION`, `XRT_UNAVAILABLE`, `DRIVER_UNAVAILABLE`,
`FIRMWARE_UNAVAILABLE_OR_UNKNOWN`, `TOOLCHAIN_UNAVAILABLE`,
`RUNTIME_VERSION_MISMATCH`, and `DEVICE_OPEN_FAILED`. Missing artifacts,
invalid buffers, context creation, synchronization, device execution, and
CPU/NPU output mismatch have separate typed runtime errors. The successful
path never executes the CPU transform as a fallback; the CPU is only the
independent expected-output oracle.

An NPU mismatch or failed canonical verification may not submit the result.
A CPU fallback, if later added, must be explicit in logs and benchmark labels.
# Active Pearl implementation note

The active architecture is the Pearl boundary in `docs/pearl/ARCHITECTURE.md`:
CPU owns job freshness, protocol parsing, proof/opening verification, target
checks, submission policy, and shutdown; the project-owned AIE2 kernel owns
only deterministic signed-int8 GEMM; XRT artifacts are built for one, two, or
four columns and are compared against the P1 CPU oracle. The current physical
implementation is P2 `4x64x8`, generalized by the P3/P5 host pipeline over
rank chunks. The official useful-work tensor provider and ZK/certificate
runtime remain external boundaries. The Qubic architecture below is frozen.

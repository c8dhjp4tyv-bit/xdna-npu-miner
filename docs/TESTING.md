# Testing Strategy

Testing is correctness-first. M1 provides the scalar CPU oracle and its
deterministic corpus. M2 verifies the standalone XDNA1/AIE2 runtime boundary;
M3 verifies one physical BPP9000 K1 recurrent tick; M4 verifies repeated ticks,
window scoring, full score reduction, and candidate lifecycle behavior against
the exact M1 oracle. Performance remains outside M4.

## Authoritative behavior under test

The current target is BPP9000 from Qubic core
`v1.301.3` (`a83f935406cd006b5b1a94971139e74d410ecb6d`). Production
dimensions are `N=18`, `M=1`, `T=8760`, `W=672`, `P=64`, `K=3`,
`S=100`, `L=1..10`, 8,088 windows, and 100,000 maximum ticks per window.
The source-derived behavior and task serialization are recorded in
`docs/UPSTREAM.md`; the readable scalar reference in the upstream test tree
is a behavioral cross-check, not code to copy.

BPP9000 uses unsigned trits `0,1,2`, with `2` meaning UNKNOWN. It has no
negative trit and no score saturation arithmetic. Any test fixture using
“negative values” must be a parser/domain rejection test, not a scorer input.

## Test classes

### 1. Pure parsing and serialization

Test the packed 96-byte header, topology, and row-major task data:

- exact little-endian widths and header size;
- dimensions and expected total length;
- input/output/signal/neighbor index ranges;
- trit unpacking and repacking;
- invalid packed byte `>=243`;
- canonical topology/data hash verification;
- trailing bytes and truncated files;
- explicit alignment/stride behavior without relying on host struct padding.

The production fixture is `qubic/core/data/bpp9000.task` at the pinned core
revision. The Qiner example file must be labeled non-canonical and must not
silently replace the production fixture.

### 2. CPU golden-reference tests (M1)

The M1 reference exposes pure, fixed-width C++ APIs equivalent to:

```text
load_task(bytes) -> validated Bpp9000Task
score_candidate(task, public_key, mining_seed, nonce, pool) -> ScoreResult
mutate_lut(lut, mutation_seed) -> changed LUT entry
is_valid_score(score) -> bool
is_good_score(score, threshold) -> bool
```

The implemented scalar components are:

- validated task parser/unpacker;
- deterministic random2 pool/draw helper with an injectable small test pool;
- exact BPP9000 LUT initialization;
- double-buffered recurrent tick;
- one-window and full-window score loops;
- mutation and accept/rollback control;
- canonical nonce validation;
- `uint32_t` score/timeout predicates;
- deterministic vector generation and vector serialization;
- a seed-aware random2/K12 draw seam with explicit 64-byte padded draw sizes;
- a `CandidateResult` containing the initial/best score, current/best LUT,
  mutation records, accept/rollback decisions, and score-call count.

M1 must favor readability and independent control flow over speed. Do not add
AVX merely because upstream core has AVX2/AVX512 paths. The CPU oracle must not
call the future NPU backend.

### 3. Deterministic vector strategy

Use two vector tiers, both generated without upstream task bytes:

1. **Small generated vectors:** fixed tiny dimensions satisfying the same
   semantic constraints (especially `K=3`, `M=1` for full-path tests), fixed
   topology, fixed packed trits, fixed public key, mining seed, and nonce.
   Include a one-window/no-settle case and a bounded-settle case.
2. **Production-shaped vectors:** independently generated 44,744-byte fixtures
   with `N=18, M=1, T=8760, P=64, K=3`, fixed topology/data metadata, and
   selected windows. M1 verifies all ten fixtures through parsing and one
   complete 672-sample window; it does not claim ten full production scores.
   The canonical production task remains source-pinned but is not committed.
   Do not substitute Qiner's example hashes for the core production task.

The task parser accepts an injected `BlockDigestProvider`. M1's
`DeterministicFixtureDigest` is a non-cryptographic test fingerprint only; it
exists to exercise header metadata and block-integrity failure paths. The
production KangarooTwelve implementation is deliberately deferred to a
reviewed future boundary and is not silently substituted by the fixture.

Cross-check scalar behavior against the upstream core reference test semantics
and Qiner's current reference miner behavior without copying implementation
structure. If an upstream vector uses a different task/configuration, record
that identity beside the result.

### 4. CPU/NPU differential tests

For identical logical inputs:

```text
CPU reference output == NPU output
```

Compare exact bytes and fields:

- state/trit arrays when exposed;
- score `uint32_t`;
- timeout/status;
- candidate/batch ordering;
- optional settled masks and iteration counts.

A padded device layout requires an explicit pack/unpack round trip and a test
that padding bytes do not affect logical output. Tolerances are forbidden.

### 5. Required correctness matrix

| Case | Required assertion |
|---|---|
| Smallest valid input | one valid window/task completes with exact state/score |
| All zero trits | CPU and NPU preserve zero semantics; no uninitialized bytes |
| Positive trits | values 1 and 2 are distinguished; 2 remains UNKNOWN |
| Negative values | rejected by parser/domain validation; never cast to a trit |
| LUT index boundaries | indices 0 and 26 select correct logical entries; 27+ is impossible/rejected |
| Mutation boundaries | old values 0, 1, 2 each change to one of the other two values |
| Saturation/clamp boundaries | verify there is no hidden saturation; score max is 8,088 and timeout is `0xffffffff` |
| Maximum accumulation | counts and tick counters remain exact `uint32_t`; no wraparound |
| Deterministic random vectors | same bytes and seed produce same pool/draw/LUT/score |
| Multiple candidates | batch order preserved; each candidate isolated |
| Multiple iterations | previous/next buffers and mutation rollback are exact |
| Full reference path | all windows, 101 score calls, threshold predicate and timeout behavior |
| Stale work | zero/stale mining seed rejected before scoring/submission |
| Noncanonical nonce | `nonce[0]`, `nonce[1]`, and `nonce[2]` rules enforced exactly |

“Positive trits” is terminology for the valid unsigned values; this algorithm
does not use signed positive/negative arithmetic.

### 6. Hardware smoke tests (M2)

Verify on the target machine:

- device identity is `RyzenAI-npu1`;
- runtime/kernel/firmware/XRT compatibility is reported;
- a deterministic tiny program executes;
- output equals a CPU expected value;
- XRT/runtime counters or equivalent evidence prove actual NPU dispatch;
- missing device/runtime fails explicitly.

A configured NPU mode without dispatch evidence is a failed test, not a pass.

The project-owned commands are:

```bash
./scripts/verify-xdna1.sh
./scripts/build-xdna-smoke.sh
./scripts/run-xdna-smoke.sh --iterations 1
./scripts/run-xdna-smoke.sh --iterations 100
python3 -m json.tool docs/evidence/m2-xdna-smoke.json
```

The verified host reports `RyzenAI-npu1`, `aie2`, BDF `0000:06:00.1`,
firmware `1.5.5.391`, XRT `2.26.0`, and the current amdxdna/kernel string in
`runtime-pins.json`. The artifact is an Iron/MLIR-AIE one-column `MLIR_AIE`
program computing `out[i] = 3 * in[i] + 7` for 32 `int32` values. The
100-dispatch run completed 100 XRT kernel waits with 100 exact matches, zero
mismatches, zero runtime failures, 200 explicit H2D synchronizations, and 100
explicit D2H synchronizations. The JSON record also states
`hardware_context_created: true` and `silent_cpu_fallback: false`.

The smoke buffer contract is exactly 32 `int32` elements, 128 bytes, 4-byte
alignment, host-contiguous input/output, explicit input and sentinel-output
H2D sync before dispatch, and explicit output D2H sync after completion. Pure
contract tests cover valid, zero-length, wrong-count, wrong-byte-count, and
misaligned cases. Each dispatch reuses allocated BOs but rewrites both input
and output before synchronization, so stale output cannot satisfy a match.

M2 does not claim four-column execution, timing, throughput, speedup, power,
or profitability.

### 6a. M2 negative and fail-closed tests

These commands are expected to return nonzero:

```bash
./build/xdna_probe --selector 99
./scripts/verify-xdna1.sh --selector 99
./build/xdna_smoke --xclbin build/xdna-smoke/xdna_smoke.xclbin \
  --insts build/xdna-smoke/xdna_smoke.insts --selector 99
./build/xdna_smoke --xclbin /does/not/exist --insts /does/not/exist
./build/xdna_smoke --xclbin build/xdna-smoke/SHA256SUMS \
  --insts build/xdna-smoke/xdna_smoke.insts
./build/xdna_smoke --xclbin build/xdna-smoke/xdna_smoke.xclbin \
  --insts build/xdna-smoke/xdna_smoke.insts --elements 31
./build/xdna_smoke --xclbin build/xdna-smoke/xdna_smoke.xclbin \
  --insts build/xdna-smoke/xdna_smoke.insts --iterations 0
```

Observed classifications are respectively `DEVICE_OPEN_FAILED`, fail-closed
capability failure, `DEVICE_OPEN_FAILED`, `ARTIFACT_MISSING`,
`ARTIFACT_INVALID`, `INVALID_BUFFER`, and `INVALID_ARGUMENT`. A physical
wrong-generation device is not present, so that status is covered by identity
rejection logic but not by a live second-device run. Context-creation,
device-execution, synchronization, and output-mismatch failures have typed
paths; they were not fabricated on a healthy device. No failure path invokes
the CPU transform or prints NPU success.

### 7. M3 K1 physical differential tests

M3 uses `src/bpp9000/reference.cpp::recurrent_tick` as the trusted CPU
expected-output path and `xdna_k1_differential` for the physical XRT path. The
host contract is 64 state trits, 46 LUT rows with 32-byte storage stride and
27 logical entries, a 64×3 `uint32_t` neighbor array, and 46 ascending
updated-neuron rows. Logical outputs are the first 64 bytes of the 96-byte
device output. State and output padding, LUT padding entries 27..31, repeated
identical dispatches, and alternate padding are explicitly tested.

The complete acceptance command is:

```bash
./scripts/run-m3-validation.sh
```

The final physical run exercised 37 edge cases, 100 fixed cases, and 1,000
seeded random cases from generator `m3-k1-v1`, random seed
`5562880460839399681`, for 1,139 physical dispatches. It reported 1,139 exact
logical matches, zero logical mismatches, zero runtime failures, 2,278 H2D
synchronizations, and 1,139 D2H synchronizations. The saved evidence is
`docs/evidence/m3-k1-differential.json`; a mismatch would be written as a JSON
record under `build/m3-mismatches/` with vector identity, generator seed/index,
logical inputs, expected/actual state, differing indices, and artifact
metadata. The same command also reruns the M1 corpus, M2 100-dispatch smoke,
and missing-artifact, invalid-selector, and wrong-manifest negative paths.

M3 proves one isolated physical tick per host dispatch. It does not prove
device-resident multi-tick state, full-window scoring, mutation/search,
multi-candidate batching, four-column execution, timing, throughput, or
profitability.

### 8. Full differential gates (M4)

M4 compares the verified K1 composition against the scalar `score_window` and
`score_lut` paths. The complete physical command is:

```bash
./scripts/run-m4-validation.sh
```

The final run exercised 1,000 repeated-tick cases, 100 one-window cases,
1,000 multi-window cases across three positions, 100 fixed cases, and 1,000
seeded random cases from seed `5562880460839399681`. It also ran 10 generated
full-score cases, one production-shaped `T=8760/W=672` full score over all
8,088 windows, and two sequential deterministic 101-score-call candidate
lifecycles. It completed 13,460 physical dispatches with exact CPU/NPU
agreement, zero mismatches, and zero runtime failures. The final evidence is
`docs/evidence/m4-full-score-differential.json`.

M4 compares exact state, score, status, timeout sentinel, tick count, feed
count, predicted output, expected output, candidate score-call count, and
final LUT/candidate state. The CPU controls mutation materialization,
accept/rollback, and final authority; an NPU-only result cannot be verified or
submitted. A mismatch writes reproducible fixture/topology/LUT/result and
artifact/runtime metadata, including the first window and candidate score-call
location when known, then fails the case.

Pure M4 negative coverage includes invalid trits, invalid topology, malformed
input/target sequences, invalid window index, excessive maximum ticks, packed
contract corruption, timeout transport, and score/status mismatch. The full
script additionally checks missing artifact, invalid device selector, and
wrong manifest; all three must fail closed. M4 intentionally does not claim
multiple batch sizes or four-column execution; those belong to M5.

### 8a. M5 batching and column tests

M5 treats one independent candidate/window pair as one work item. The pure
M5 contract test must verify exact item and result strides, candidate/window
metadata, per-item status/error transport, output-slot ordering, and stale
output rejection. The physical M5 differential test must run deliberately
different lanes, then reverse the assignment and the `A,A,B,A` pattern; every
slot must equal the independently recomputed M1 `score_window` result.

Accepted artifacts are fixed `(batch_size, columns)` variants. Required
physical configurations are batch sizes 1, 2, and 4 plus a larger batch when
the device permits, and column counts 1, 2, and 4. A batch must be divisible
by its column count. Generated placement and unique input/result evidence are
required before calling a column active. Buffer reuse tests must prove that
input/output BO reuse, mutation-visible LUT replacement, reset state, and
sentinel output initialization do not change exact results.

M5 benchmark runs use at least two warm-ups and five measured repeats over an
identical deterministic window-item corpus. They record raw wall samples,
median/p95 where practical, physical dispatches, H2D/D2H syncs and bytes,
dispatch/wait time when available, host preparation, CPU verification, and
zero mismatch/runtime failures. The M4 one-dispatch path is the control for
the same item corpus. No CPU fallback, result synthesis, or reordering is
permitted. The physical runner also performs a host LUT mutation, dispatches
the changed candidate, rolls the mutation back, dispatches again, and checks
that the original LUT bytes are restored.

The full reproducible M5 command is:

```bash
./scripts/run-m5-validation.sh
python3 -m json.tool docs/evidence/m5-batching-four-column.json
```

The final matrix accepted `(1,1)`, `(2,1)`, `(4,1)`, `(2,2)`, `(4,2)`,
`(8,2)`, `(4,4)`, `(8,4)`, and `(16,4)` as `(batch_size, columns)` pairs.
Every configuration produced 80/80 exact measured item matches and zero
mismatches/runtime failures. Generated `npu1_2col` and `npu1_4col` partition
metadata, lane-to-item ranges, ordered/reversed/A,A,B,A isolation patterns,
and unique per-lane results are saved in the aggregate evidence. The selected
raw-timing configuration was batch 16/four columns; M5 remains CPU-authorized
only after exact recomputation.

### 9. Protocol/integration tests (M6/M7)

The direct-node test boundary is the required first M6 integration path:

- type-0 peer-exchange handshake plus 8-byte request/response frame parsing;
- system-info response extraction of epoch/tick/seed/threshold;
- current-computor request/response framing with exact epoch, key-count, and
  nonzero-key checks;
- task hash and algorithm selection;
- signed/encrypted solution broadcast construction;
- exact claimed-score recomputation;
- stale seed, invalid nonce, bad score and threshold rejection;
- reconnect and timeout behavior.

Use captured/mock packets and an authorized live endpoint only. M1 through M5
must not depend on Qatum or any mining pool. Qatum/pool tests are optional after
the direct-node path works and remain deferred until a stable authoritative,
sufficiently complete protocol revision or implementation is pinned and
independently reviewed; never invent pool frames. Never log private signing
seeds or access tokens.

The offline live-probe harness exercises a local mock success, wrong-frame,
truncated-frame, and timeout server:

```bash
./scripts/test-qubic-live-probe-offline.sh
```

The bounded public read-only interoperability probe uses the official endpoint
explicitly; it never loads signing material or submits a frame:

```bash
./scripts/run-m6-live-system-info.sh
```

The bounded current-destination probe uses the public `REQUEST_COMPUTORS`
(`type=11`) path and accepts only an exact `BROADCAST_COMPUTORS` (`type=2`)
payload. It is also read-only and does not claim Arbitrator-signature
verification:

```bash
./scripts/run-m6-live-computors.sh
```

The 2026-08-10 run returned epoch 225, 676 nonzero public keys, and a
21698-byte payload from `corenet.qubic.li:21841`; sanitized digests and the
signature-verification limitation are recorded in
`docs/evidence/m6-direct-node.json`. The production task is not returned by
SystemInfo; its current hash-verified source is the pinned upstream
`data/bpp9000.task` input documented in `docs/UPSTREAM.md`.

The guarded submission surface is intentionally non-operative without an
authorized identity and candidate. `scripts/run-m6-live-submit.sh` exits
nonzero and records the external protocol requirement without sending a
frame; it must not be converted into an M7 supervisor.

### 10. Endurance/recovery tests (M10)

Exercise epoch/seed changes, reconnects, device errors, queue restarts, bounded
memory, graceful stop, and submission rejection. A release endurance run must
define a duration (minimum 24 hours for the release gate), workload, hardware,
and expected zero silent correctness errors.

## Verification commands

M1 uses a standalone C++20/CMake build and a dependency-free test executable:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/bpp9000_tests
./scripts/generate_corpus.sh build
```

The passing M1 executable reports 8 test groups and 361 assertions. The
standalone corpus command reports 100 generated cases and 10
production-shaped cases with generator version `m1-v1` and these reproducible
fixture-summary digests:

```text
generated_digest=2979889feed3352b3c12831a301a357b6c9099f3de80b955f152c53bca2f8c03
production_digest=7c1da1028b9ecdbae54616654606185e62076ff7b69e209ecbf3d23f6a2fede1
```

The corpus summary is a committed expected value, not a performance result.
The tests were run twice during M1 verification; both runs produced the same
summary values.

The M2 contract test is included in `ctest` and reports five assertions. The
hardware evidence file is machine-readable and is intentionally a record of
the current host, not a portable support claim. The exact project-owned pins
are in `runtime-pins.json`.

The M3-specific contract test is also included in `ctest` and reports 52
assertions. The full physical validation command above rebuilds the K1
artifact, validates the current pinned XDNA stack, regenerates the M1 corpus,
reruns M2, checks expected negative failures, and writes machine-readable K1
evidence. Validate that record with:

```bash
python3 -m json.tool docs/evidence/m3-k1-differential.json
git diff --check
```

The M4 contract test is included in `ctest`. The full M4 validation command
rebuilds the one-column score artifact, reruns all M1/M2/M3 regressions,
executes the physical M4 tiers and negative paths, and writes the score
evidence record. Validate it with:

```bash
python3 -m json.tool docs/evidence/m4-full-score-differential.json
```

The M6 direct-node contract is included in `ctest` as
`qubic_direct_node_tests`. It uses a chunked in-memory byte stream and a
non-cryptographic test provider only. Coverage includes strict 8-byte frame
parsing, partial reads, all 128-byte system-info fields, bounded reconnect,
epoch/tick/seed freshness, task/algorithm compatibility, exact CPU/NPU score
comparison, canonical nonce and threshold gates, deterministic solution
serialization, explicit live-submission opt-in, and secret-redacted runtime
configuration. The required no-send cases are stale seed/context,
CPU/NPU mismatch, invalid nonce, unsupported algorithm, task mismatch, bad
threshold, timeout sentinel, malformed context, and disabled live submission.

Run the offline M6 boundary with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 -m json.tool docs/evidence/m6-direct-node.json
```

The optional production crypto gate is separate from the default build:

```bash
cmake -S . -B build-crypto -DCMAKE_BUILD_TYPE=Debug \
  -DXDNA_ENABLE_PRODUCTION_CRYPTO=ON
cmake --build build-crypto -j2
ctest --test-dir build-crypto --output-on-failure
```

This opt-in run includes `qubic_crypto_tests`, which checks RFC 9861 K12
vectors, the synthetic Qubic SchnorrQ signature vector, and exact synthetic
FourQ public-key, shared-key, gamming-key, and gamma-stream bytes. The
observed gate is 7/7 tests passed. No signing secret is implied by either test
suite. Live system-info has a recorded read-only PASS against
`corenet.qubic.li:21841`; solution submission remains disabled until an
authorized source identity, current computor destination, current task
compatibility, eligible CPU/NPU candidate, and safe runtime signing material
are supplied.

## Security-oriented tests

Ensure later code does not:

- expose signing seeds or pool tokens in logs;
- accept unauthorized remote-control behavior;
- continue after explicit shutdown;
- silently change configured endpoints;
- submit a result without CPU canonical verification;
- report CPU fallback as NPU execution;
- submit stale-seed or invalid-score work.

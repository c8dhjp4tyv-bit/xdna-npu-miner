# Testing Strategy

Testing is correctness-first. M1 provides the scalar CPU oracle and its
deterministic corpus. M2 now verifies a tiny standalone XDNA1/AIE2 dispatch
against an exact CPU oracle; BPP9000 XDNA work and performance remain outside
this milestone.

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

### 7. Kernel and full differential gates (M3/M4)

M3 must compare the first selected primitive against the scalar oracle for all
boundary cases and a deterministic random corpus. M4 must compare full scores
and timeout/status for at least:

- every required matrix row above;
- 100 fixed generated cases;
- 1,000 seeded random cases, with the seed and corpus generator version
  recorded;
- multiple candidates and multiple batch sizes;
- at least one production-shaped task case;
- every mismatch saved with task hash, public key, seed, nonce, candidate index,
  batch size, and serialized inputs.

The exact counts are acceptance gates for the initial implementation; a later
milestone may expand them but may not reduce them without a recorded decision.

### 8. Protocol/integration tests (M6/M7)

The direct-node test boundary is the required first M6 integration path:

- 8-byte request/response frame parsing;
- system-info response extraction of epoch/tick/seed/threshold;
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

### 9. Endurance/recovery tests (M10)

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

## Security-oriented tests

Ensure later code does not:

- expose signing seeds or pool tokens in logs;
- accept unauthorized remote-control behavior;
- continue after explicit shutdown;
- silently change configured endpoints;
- submit a result without CPU canonical verification;
- report CPU fallback as NPU execution;
- submit stale-seed or invalid-score work.

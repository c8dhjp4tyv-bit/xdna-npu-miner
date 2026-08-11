# Benchmark Protocol

M0 defines measurement methodology. M1 and M2 established correctness
evidence, M3 established physical K1 dispatch evidence, and M4 established
physical full-score correctness evidence. No throughput, latency, power,
NPU-activity, speedup, or work/Joule value may be inferred from those
correctness runs.

## Pearl P1 benchmark boundary

Pearl P0 contains only a first-order arithmetic/traffic model, and P1 adds
only CPU correctness vectors, in
[`docs/pearl/PROJECT_SPEC.md`](pearl/PROJECT_SPEC.md) and
[`docs/evidence/pearl-p0.json`](evidence/pearl-p0.json). P1 contains no Pearl
throughput, latency, hashrate, power, energy, four-column, NPU, or
profitability measurement. A Pearl benchmark may begin only after the P1 CPU
oracle and P2 XDNA1 differential contract pass, with Pearl source/version,
dimensions, rank, batch, warm-up, repetitions, correctness, telemetry, and
hardware recorded.

## Workload identity

Every result must identify:

- repository commit and milestone;
- Qubic algorithm revision, currently BPP9000 from core
  `v1.301.3` / `a83f935406cd006b5b1a94971139e74d410ecb6d`;
- task file SHA-256 and topology/data hashes;
- public key, mining seed and nonce corpus identifiers without exposing
  signing secrets;
- production dimensions (`N=18, M=1, T=8760, W=672, P=64, K=3, S=100,
  L=1..10`) or the exact reduced test dimensions;
- candidate definition: one full candidate search, including initial score and
  100 mutation attempts, unless another unit is explicitly labeled;
- batch size and four-column mapping.

The Qiner example task and its 6,469 example threshold must never be mixed
silently with the core production task and runtime threshold.

## Required comparisons

The same task bytes, candidate corpus, correctness status, and requested work
must be used for each available placement:

| Placement | Status | Candidate scores/s | Score calls/s | Windows/s | Batch |
|---|---|---:|---:|---:|---:|
| CPU scalar reference | Not measured | — | — | — | — |
| CPU optimized | Not measured | — | — | — | — |
| XDNA1 recurrent/fused | Not measured | — | — | — | — |
| CPU + XDNA1 hybrid | Not measured | — | — | — | — |

A “candidate” is the complete defined search. Also report algorithm-native
subunits (`score calls/s`, `windows/s`, and `ticks/s`) when a partial kernel is
benchmarked; partial-kernel numbers must not be presented as candidate mining
throughput.

## Metrics

For every run, record:

- candidate scores/sec and algorithm-native work/sec;
- latency per candidate and per batch: median, p95, and sample count;
- batch size, queue depth, warm-up count, and steady-state count;
- CPU utilization by process and, where useful, core placement;
- host RAM/high-water mark;
- XRT/NPU dispatch evidence and available per-kernel activity/counters;
- host-to-device and device-to-host transfer time;
- kernel execution time and host scheduling/launch time;
- errors, timeouts, dropped/retried candidates, and result correctness;
- wall power or energy only from a named measurement source;
- work/Joule only when both work and energy are measured over the same interval;
- thermal/clocks and power profile when available.

If a telemetry source is unavailable, write “not available” with the reason.
Configured offload, a device name, or a successful CPU result is not NPU
activity evidence.

## Method

1. Confirm the exact task/corpus and run the full correctness suite first.
2. Record clean git state or the exact working-tree diff, hardware identity,
   Fedora/kernel/firmware/XRT/MLIR-AIE/IRON versions, and device name.
3. Warm up compilation, program load, buffer allocation, and caches separately.
4. Measure steady-state runs with fixed batch size and queue configuration;
   report cold-start/load latency separately.
5. Repeat enough times to produce median and p95; preserve raw samples.
6. Keep CPU governor/power mode, thermals, affinity, task bytes and candidate
   order controlled between comparisons.
7. Measure transfer and kernel intervals with the same clock/source where
   possible; state timestamp limitations.
8. Confirm actual XDNA dispatch for every result labeled XDNA1.
9. Stop and classify any correctness mismatch; do not average failed results.
10. Store a machine-readable record plus a human-readable summary under
    `benchmarks/` once implementation exists.

The first NPU benchmark must include a CPU reference run on the identical logical
inputs and must state whether it measured K1, K2, K3, or a fused composition.

## Acceptance gates by metric

- A throughput result is publishable only after exact CPU/NPU correctness passes
  for the same corpus.
- A four-column result requires evidence that all claimed columns were active
  or an explicit statement that the mapping was not verified.
- A power/energy result requires the instrument or counter name, sampling
  interval, units, and uncertainty/limitations.
- A work/Joule result cannot be derived from TDP, utilization percentage, or
  marketing specifications.
- A speedup claim must include CPU baseline, NPU/hybrid configuration, workload
  identity, and error count.

## Current evidence

M0 and M1 executed no benchmark. M2 executed a correctness smoke only: the
current physical `RyzenAI-npu1`/AIE2 device completed 100 one-column XRT
dispatches for `int32[32]`, with 100 exact CPU-oracle matches, zero mismatches,
zero runtime failures, 200 explicit H2D synchronizations, and 100 explicit D2H
synchronizations. The machine-readable record is
`docs/evidence/m2-xdna-smoke.json`.

M3 executed a correctness differential only: the same physical device
completed 1,139 one-column K1 XRT dispatches over 37 edge, 100 fixed, and
1,000 seeded-random logical cases. All 1,139 outputs matched the M1 scalar
`recurrent_tick` oracle exactly; there were zero mismatches and zero runtime
failures, with 2,278 H2D and 1,139 D2H synchronizations. The machine-readable
record is `docs/evidence/m3-k1-differential.json`; the stack and artifact pins
are recorded in that file and `runtime-pins.json`.

M4 executed a correctness differential only: the same physical device
completed 13,460 one-column XRT dispatches for repeated ticks, one-window and
multi-window cases, 11 full scores including one production-shaped 8,088-window
score, and two 101-score-call candidate lifecycles. All dispatches completed;
the CPU/NPU path recorded zero mismatches and zero runtime failures, with
26,920 H2D and 13,460 D2H synchronizations. The machine-readable record is
`docs/evidence/m4-full-score-differential.json`. Its diagnostic duration and
dispatch counts are correctness-run evidence only.

These are dispatch/correctness records, not benchmarks. No timing, throughput,
speedup, power, energy, active-four-column claim, or profitability value was
recorded. M4 used one column, one physical dispatch per operation, host
round-trips for each result, and no persistent state/topology/LUT reuse. The
table above remains intentionally unmeasured. Do not infer a performance claim
from the static operation counts in `docs/ARCHITECTURE.md`, from any dispatch
count or validation duration, or from the related `hawkpoint-npu-llm` project.

## M5 measurement contract

M5 compares the M4 one-window control with a batched artifact over the same
deterministic item list. One item is one independent candidate/window pair;
the complete candidate mutation search is not silently represented as a
partial throughput number. Each result is CPU-recomputed with the M1 oracle
in both control and batched paths.

For every fixed `(batch_size, columns)` artifact, record the artifact and
instruction hashes, generated placement, batch/candidate/window identity,
warm-up count, measured repeat count, raw wall samples, median/p95, host
preparation, XRT dispatch/wait when available, CPU verification/reduction,
physical dispatches, H2D/D2H sync counts and bytes, active-lane evidence,
matches, mismatches, and runtime failures. XRT BOs may be reused only with a
full input rewrite and output sentinel rewrite per dispatch. A rejected or
unsupported compiler mapping is recorded with its exact configuration and
error; it is not converted to a timing result.

M5 may report raw `window-items/sec` or `score-window operations/sec`. It may
not call this candidate hashrate, convert it into profitability, or claim a
speedup unless the logical work, CPU verification, warm-up/repeat method, and
raw values are identical to the M4 control.

## M5 measured raw results

The completed M5 run used the physical `RyzenAI-npu1` / AIE2 device, Fedora
runtime pins from `runtime-pins.json`, 16 deterministic independent
candidate/window pairs, two warm-ups, and five measured repeats. The M4
reference was run first on the same 16 items with one dispatch per item. The
baseline median was 2.987789 ms (p95 3.251935 ms), 80 physical dispatches,
160 H2D syncs, 1,246,800 H2D bytes, 80 D2H syncs, and 10,240 D2H bytes.

| Batch / columns | Median ms (p95) | Dispatches | H2D syncs / bytes | D2H syncs / bytes | Exact items / mismatches |
|---:|---:|---:|---:|---:|---:|
| 1 / 1 | 3.610288 (4.909527) | 80 | 160 / 1,249,280 | 80 / 10,240 | 80 / 0 |
| 2 / 1 | 3.093838 (3.938564) | 40 | 80 / 1,249,280 | 40 / 10,240 | 80 / 0 |
| 4 / 1 | 1.965079 (2.214187) | 20 | 40 / 1,249,280 | 20 / 10,240 | 80 / 0 |
| 2 / 2 | 2.489644 (2.997457) | 40 | 80 / 1,249,280 | 40 / 10,240 | 80 / 0 |
| 4 / 2 | 1.694662 (1.838581) | 20 | 40 / 1,249,280 | 20 / 10,240 | 80 / 0 |
| 8 / 2 | 1.518220 (1.920385) | 10 | 20 / 1,249,280 | 10 / 10,240 | 80 / 0 |
| 4 / 4 | 1.792676 (1.981721) | 20 | 40 / 1,249,280 | 20 / 10,240 | 80 / 0 |
| 8 / 4 | 1.399127 (1.929262) | 10 | 20 / 1,249,280 | 10 / 10,240 | 80 / 0 |
| 16 / 4 | 1.277969 (1.516637) | 5 | 10 / 1,249,280 | 5 / 10,240 | 80 / 0 |

Batch 16 / four columns is the selected configuration by lowest measured
median wall time. It reduces physical dispatches from 80 to 5 and H2D/D2H
synchronization calls from 160/80 to 10/5 for this identical 16-item measured
workload. H2D bytes are slightly larger than M4 because M5 deliberately rounds
each 15,457-byte logical M4 payload to a 15,488-byte fixed device stride; D2H
bytes are unchanged. These are raw timing and transfer observations; the
machine-readable records intentionally set `speedup_claim:false` and contain
all five raw samples per configuration.

The generated AIE metadata reports partition width 1, 2, or 4 for the
respective artifact families. For four-column runs it reports four row-2 core
workers, and the isolation corpus gives each lane distinct inputs plus exact
CPU-verified results. No NPU telemetry counter was available beyond physical
XRT dispatch completion. No power, energy, profitability, or hashrate claim
is made. Full evidence is in
`docs/evidence/m5-batching-four-column.json`; the sweep is reproducible with
`./scripts/run-m5-validation.sh`.

## Profitability

Profitability is separate from hardware throughput. If ever calculated, record
the timestamp, reward/difficulty source, token price source, electricity price,
fees, pool terms, and assumptions; do not mix it into the benchmark gate.

## M6 protocol note

M6 direct-node framing, freshness, serialization, mock transport, optional
K12/FourQ KATs, and the bounded public system-info probe are correctness/
interoperability evidence only. The probe uses the official node for read-only
system info; it is not a throughput, mining-rate, latency, power, energy, or
profitability benchmark and does not submit a solution. No production signing
secret was used. The machine-readable state is
`docs/evidence/m6-direct-node.json`.
# Pearl benchmark snapshot (2026-08-11)

The Pearl P8/P9 records are the active benchmark claims. On the physical
`RyzenAI-npu1` / AIE2 device (BDF `0000:06:00.1`, firmware `1.5.5.391`, XRT
`2.26.0`, observed amdxdna rc7), a fixed `4x64x8` int8→int32 corpus selected
four columns and batch eight at the best measured raw dispatch throughput.
The final P9 run measured 7.205k exact raw dispatches/s and 3.01093 full
candidate/s for `K=2048`, rank 128, with zero mismatches and zero CPU
fallbacks. The CPU baseline, timing methodology, high-water RAM, null power,
and null NPU telemetry are in `docs/evidence/pearl-p8-batching-four-column.json`
and `docs/evidence/pearl-p9-benchmark.json`. These are not profitability or
income claims; gateway/prover overhead was not measured as a performance
benchmark. P7 interoperability is a correctness result: one bounded
physical-XDNA search found an official-accepted proof on attempt 21, with no
hashrate, power, energy, or profitability number claimed. P10 is an
endurance/correctness result, not a throughput benchmark.

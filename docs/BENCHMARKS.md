# Benchmark Protocol

M0 defines measurement methodology. M1 and M2 established correctness
evidence, and M3 established physical K1 dispatch evidence. No throughput,
latency, power, NPU-activity, speedup, or work/Joule value may be inferred
from those correctness runs.

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

These are dispatch/correctness records, not benchmarks. No timing, throughput,
speedup, power, energy, active-four-column claim, or profitability value was
recorded. The table above remains intentionally unmeasured. Do not infer a
performance claim from the static operation counts in `docs/ARCHITECTURE.md`,
from either dispatch count, or from the related `hawkpoint-npu-llm` project.

## Profitability

Profitability is separate from hardware throughput. If ever calculated, record
the timestamp, reward/difficulty source, token price source, electricity price,
fees, pool terms, and assumptions; do not mix it into the benchmark gate.

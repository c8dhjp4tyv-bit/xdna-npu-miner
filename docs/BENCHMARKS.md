# Benchmark Protocol

Do not publish performance claims without recorded methodology and reproducible evidence.

## Required metadata

Every benchmark record should include:

- project commit;
- milestone;
- exact hardware model;
- XDNA device identity;
- kernel/firmware/XRT/MLIR-AIE/IRON versions as applicable;
- CPU model and power profile;
- workload/algorithm version;
- input dimensions;
- batch size;
- warm-up procedure;
- number of measured iterations;
- correctness status;
- failures/timeouts;
- host RAM;
- NPU telemetry when available;
- power/energy source and methodology when available.

## Primary metrics

Depending on the final Qubic workload terminology established in M0, report the most appropriate unit such as candidates/s, scores/s, iterations/s, or another protocol-meaningful throughput metric.

Also record:

- median latency;
- p95 latency where meaningful;
- throughput;
- host/NPU transfer time when measurable;
- kernel execution time when measurable;
- CPU utilization;
- memory use;
- NPU activity/telemetry;
- wall power or energy only if measured reliably;
- error count.

## Comparative table

Do not fill cells until actually measured under comparable conditions.

| Placement | Throughput | Median latency | p95 | RAM | NPU activity | Power/Energy | Errors |
|---|---:|---:|---:|---:|---:|---:|---:|
| CPU reference | Not measured | — | — | — | — | — | — |
| XDNA1 accelerated | Not measured | — | — | — | — | — | — |
| CPU + XDNA1 hybrid | Not measured | — | — | — | — | — | — |

## Benchmark rules

1. Correctness tests must pass before performance is accepted.
2. Use identical workload inputs for placement comparisons.
3. Keep power profile, thermals and warm-up conditions as consistent as practical.
4. Do not treat configured offload as utilization; use real telemetry where available.
5. Do not infer NPU energy from TDP.
6. Do not mix cold compile/load with steady-state throughput unless explicitly reporting TTFT/startup.
7. Preserve raw benchmark artifacts for significant milestone/release claims when practical.
8. Report rejected/failed optimization experiments as such; do not cherry-pick only successful runs.

## Profitability

Profitability is outside core benchmark correctness because coin price, rewards, difficulty and electricity cost are time-varying external inputs.

If profitability is later calculated, keep it separate from hardware throughput and include the exact timestamp, market/difficulty data source, electricity price and assumptions.

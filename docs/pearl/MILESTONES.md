# Pearl (PRL) Research Milestones

Pearl milestones are independent of the completed/frozen Qubic milestones.
No later Pearl milestone may be started early, and no milestone may claim
NPU execution without exact CPU differential evidence.

P2 through P11 are executed in one continuous engineering shot on
`feat/pearl-full-miner-one-shot`. This changes only handoff cadence: every
gate below remains sequential, machine-readable, and independently PASS,
FAIL, BLOCKED, or NOT_RUN.

## One-shot gate snapshot (2026-08-11)

| Gate | Status | Evidence |
|---|---|---|
| P0 | PASS | `pearl-p0.json` |
| P1 | PASS | `pearl-p1.json` |
| P2 | PASS | `pearl-p2-xdna-matmul.json` |
| P3 | PASS | `pearl-p3-compute-pipeline.json` |
| P4 | PASS (live provider blocked) | `pearl-p4-job-integration.json` |
| P5 | PASS (official wire/prover blocked) | `pearl-p5-candidate-proof.json` |
| P6 | PASS (official endpoint blocked) | `pearl-p6-gateway.json` |
| P7 | PASS — official SIMNET accepted physical-XDNA PlainProof | `pearl-p7-e2e.json` |
| P8 | PASS | `pearl-p8-batching-four-column.json` |
| P9 | PASS | `pearl-p9-benchmark.json` |
| P10 | PASS | `pearl-p10-endurance.json` |
| P11 | PASS | `pearl-p11-delivery.json` |

The aggregate state is `SOFTWARE_COMPLETE_E2E_PASS` for the verified official
SIMNET path. Mainnet payout configuration and pool/Stratum support remain
unavailable boundaries and are not implied by this result. P10 passed its
exact 1,800-second physical run; P11 is the completed operator-delivery gate.

## P0 — Upstream, protocol, license, and XDNA feasibility baseline

**Status: COMPLETE — documentation/evidence only (2026-08-10).**

Exit criteria:

- pin the official Pearl repository, mining implementation, node/protocol
  revision, application/RPC versions, and whitepaper hash;
- trace template acquisition through matrix work, proof, and submission;
- identify exact dimensions, dtypes, quantization, noise, accumulation,
  transcript, proof, and network messages;
- classify every relevant component license and prohibit unclear source reuse;
- compare the dense primitive with XDNA1/AIE2 capabilities without claiming a
  benchmark;
- record an independently checkable first-order model and a single gate state;
- validate the P0 JSON record, documentation, and whitespace.

The P0 gate is `UNKNOWN_NEEDS_EXPERIMENT`: the dense int8 MAC primitive is
`POSSIBLE_FIT`, but no Pearl implementation has been executed on XDNA1.

## P1 — Trusted CPU golden path

**Status: COMPLETE — clean-room CPU oracle and canonical corpus (2026-08-10).**

The implementation in `src/pearl/` uses independently designed C++ types and
algorithms. The only reused external implementation boundary is an official
`blake3` 1.8.2 Rust dependency exposed through a minimal C ABI; no Pearl
hot-component source is copied or translated.

P1 defines and tests canonical header/config/public-data serialization,
quantization, checked signed products, deterministic low-rank noise,
commitment seeds, the selected 2x64 tile, transcript trace, keyed jackpot,
1024-byte Merkle openings, and a fixed-width candidate PlainProof envelope.
It resolves the raw `[-64,64]` versus quantized `[-63,63]` distinction and
rejects arithmetic overflow instead of guessing wrap/saturation behavior.

The fixed corpus is `tests/data/pearl/p1/`; the test target is
`pearl_cpu_golden_tests`. Seeded tests cover rank 32/64/128 and valid k/edge
cases, while P1 remains CPU-only.

Exit: deterministic vectors pass on repeated runs and invalid dimensions,
rank, target, layout, and overflow cases fail closed.

## P2 — Minimal XDNA1 dense matmul

Port only a bounded signed-int8 × signed-int8 → int32 tile to the existing
XDNA1 runtime. Keep input/output schemas Pearl-specific and use a batch of one
first. Do not call it Pearl mining until the CPU oracle comparison is present.

Exit: physical device identity, dispatch evidence, exact output parity, and
failure-path evidence on the target stack.

## P3 — CPU↔NPU differential transcript

Add exact noise/correction and selected reduction stages incrementally. Compare
every intermediate field and final jackpot bytes against the CPU reference;
measure transfer, dispatch, synchronization, and wait time.

Exit: no mismatches across edge and randomized vectors; CPU remains the final
authority.

## P4 — Real job parser

Parse the pinned `getMiningInfo`/gateway job schema and construct the P1 input
contract without a live submission path. Test stale templates, certificate
version changes, target/rank rejection, and all endianness boundaries.

## P5 — Scoring/proof candidate

Create and verify a `PlainProof` candidate and selected Merkle openings on CPU.
Only after the CPU proof is trusted may the NPU result feed the candidate
path. ZK generation remains CPU/Rust and must be version pinned.

## P6 — Pool/direct-node adapter

Implement only a protocol whose authoritative messages and license are
available. Direct-node and pool support are separate adapters. Do not infer
Stratum behavior from the existence of `nbits_override`.

## P7 — Bounded live operation

**Status: COMPLETE — official local SIMNET interoperability (2026-08-11).**

The pinned official `pearld` `1.3.1`, `pearl-gateway 0.1.0`, and
`py-pearl-mining 0.2.0` ran outside the repository. Official `getMiningInfo`
was parsed, a physical XDNA1 dense search found a valid jackpot, the project
CPU verifier passed, and the official bincode PlainProof was submitted through
the gateway. The gateway's block submission was accepted by `pearld`; the
physical-XDNA block is recorded in `docs/evidence/pearl-p7-e2e.json`.

The SIMNET matrix source is a deterministic interoperability fixture. It does
not claim reproduction of the official CUDA/vLLM useful-work provider. No
mainnet address or pool protocol was used or inferred. The exact checkout,
binary digest, Python/Torch isolation, wire length, block hash, and zero CPU
fallbacks are machine-recorded in the P7 evidence file.

## P8 — Batching and four-column mapping

Measure independent Pearl jobs at batch 1, then larger bounded batches, and
one/two/four XDNA1 columns under one fixed corpus. Select a configuration only
from reproducible measurements; no artificial NPU-work percentage target.

## P9 — Benchmark record

Record exact source/hardware/software stack, dimensions, rank, batch,
throughput, latency, warm-up, repetitions, correctness, RAM, NPU telemetry,
and reliable power/energy when available. Compare only materially matched
conditions and publish no unmeasured hashrate/profit number.

## P10 — Endurance and recovery

Run bounded endurance with job refresh, device errors, stale work, proof
failure, backpressure, and shutdown recovery. Keep all secrets and network
state out of evidence. Leave the system in a safe, auditable state.

## P11 — CLI, packaging, and operator delivery

Deliver a safe `pearl-xdna-miner` command with explicit `--mine` opt-in and
non-mining `--help`, `--version`, `--self-test`, `--hardware-info`,
`--benchmark`, and `--dry-run` modes. Support bounded configuration for the
gateway, node, public mining address, device, batch, columns, logging, JSON
status, and maximum runtime without logging credentials or private material.

Exit: clean build/install instructions, example configuration containing only
placeholders, CPU and physical XDNA self-test classifications, CLI/parser
tests, operator and security documentation, aggregate milestone evidence, and
a reproducible clean-checkout verification path. No autostart, persistence,
hidden mining, remote deployment, or profitability guarantee is permitted.

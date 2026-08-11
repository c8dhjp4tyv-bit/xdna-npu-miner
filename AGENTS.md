# AGENTS.md

This repository is intentionally developed by multiple AI coding agents with limited context windows. The repository itself is the authoritative project memory.

## Mandatory startup sequence

Before changing code or documentation, every agent must read:

1. `AGENTS.md`
2. `docs/AI_HANDOFF.md`
3. `docs/PROJECT_SPEC.md`
4. `docs/MILESTONES.md`
5. `docs/ARCHITECTURE.md`
6. `docs/DECISIONS.md`
7. `docs/TESTING.md`
8. `docs/UPSTREAM.md`
9. `docs/BENCHMARKS.md` when performance work is relevant

Then inspect:

- `git status`
- current branch
- recent commits
- relevant source files
- relevant tests

Do not rely on previous chat history.

## Current target

Build a standalone AMD XDNA1 NPU-accelerated Pearl (PRL) miner. Pearl is the
active target; the completed and partially integrated Qubic work is
frozen/reference-only and must not be resumed, rewritten, or used as Pearl
protocol evidence.

Pearl P0 and P1 are complete. The active execution mode is one continuous
engineering shot through P2-P11. Milestone gates remain sequential and
mandatory: a later success never hides an earlier failure, and every
unavailable external dependency is recorded as BLOCKED rather than PASS.

Primary hardware target:

- AMD Ryzen AI Hawk Point
- XDNA1 / `RyzenAI-npu1`
- AIE2
- 4 columns
- Fedora Linux

## Milestone discipline

- Work only on the current milestone unless explicitly instructed otherwise.
- Do not start later milestones early.
- Do not mark a milestone complete unless every acceptance criterion in `docs/MILESTONES.md` passes.
- Preserve verified behavior.
- Do not redesign completed architecture silently.
- Record justified architecture changes in `docs/DECISIONS.md`.
- During the Pearl full-project shot, continue automatically from one passing
  gate to the next without waiting for a new prompt. Continue all independent
  work when one area is externally blocked.

## Correctness discipline

Every accelerated operation must follow this pattern:

`input -> trusted CPU reference -> NPU implementation -> compare -> benchmark`

A faster incorrect kernel is a failed kernel.

Never:

- fabricate protocol behavior;
- fabricate hardware execution;
- fabricate benchmark numbers;
- silently fall back to CPU while claiming NPU execution;
- weaken correctness tests to make a kernel pass.

## Upstream and licensing discipline

When protocol or algorithm behavior is required:

- use current authoritative upstream sources;
- record exact source URLs/repositories, versions, commits, tags, and relevant files in `docs/UPSTREAM.md`;
- distinguish verified facts from assumptions;
- document unresolved ambiguity instead of guessing;
- do not copy code whose license is incompatible with this repository;
- prefer clean-room implementation when licensing is unclear or restrictive.

## XDNA discipline

The objective is genuine XDNA1 execution, not a cosmetic NPU mode.

Prefer:

- CPU for networking, orchestration, mutation/control flow, validation and submission when those tasks are not compute-dense;
- XDNA1 for deterministic, repeated, batchable compute kernels that fit AIE2 well;
- persistent/reused NPU buffers where practical;
- batched work to amortize host/NPU transfer overhead;
- explicit use of four XDNA1 columns when it measurably improves throughput.

Never optimize for an artificial percentage of work on the NPU.

## Benchmark discipline

Record at minimum when relevant:

- workload definition and version
- exact hardware/software stack
- batch size
- throughput
- latency
- correctness result
- host RAM
- NPU telemetry when available
- wall power/energy when reliably measurable
- failures
- warm-up methodology
- number of repetitions

Do not compare results obtained under materially different conditions without labeling the difference.

## Security and abuse boundaries

This project is for mining on hardware the operator owns or is authorized to use.

Do not implement:

- hidden or stealth mining;
- unauthorized persistence;
- credential theft;
- propagation;
- disabling security tools;
- unauthorized remote deployment;
- mechanisms intended to conceal resource consumption from the system owner.

## Mandatory handoff

Before ending work, update `docs/AI_HANDOFF.md` with:

- current milestone
- status: NOT STARTED / IN PROGRESS / BLOCKED / COMPLETE
- branch
- commit if available
- completed work
- files changed
- tests executed
- exact results
- hardware tests actually executed
- known failures
- assumptions
- architectural decisions
- things the next agent must not redo
- next exact task
- relevant commands

Also update `docs/DECISIONS.md`, `docs/TESTING.md`, `docs/BENCHMARKS.md`, `docs/ARCHITECTURE.md`, `docs/MILESTONES.md`, or `docs/UPSTREAM.md` when the work materially changes them.

## End-of-turn report

When finishing a work session, report:

1. what changed;
2. what was verified;
3. what remains;
4. current milestone status;
5. exact recommendation for the next agent.

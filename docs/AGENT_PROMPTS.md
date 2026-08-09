# Agent Prompts

Use these prompts when handing the repository to a new coding agent. Repository files remain authoritative; prompts are only entry points.

## Generic zero-context continuation prompt

```text
Continue development of the existing repository.

This project is intentionally developed by multiple AI coding agents. Do NOT assume you have useful context from previous conversations.

First recover project state entirely from the repository.

Read in this order:
1. AGENTS.md
2. docs/AI_HANDOFF.md
3. docs/PROJECT_SPEC.md
4. docs/MILESTONES.md
5. docs/ARCHITECTURE.md
6. docs/DECISIONS.md
7. docs/TESTING.md
8. docs/UPSTREAM.md
9. docs/BENCHMARKS.md if relevant

Then inspect:
- git status
- current branch
- last 10 commits
- files changed in the current milestone
- relevant tests

Do not write code until you understand:
- the current milestone,
- what is already complete,
- what is actually verified,
- what remains,
- and what the previous agent explicitly said must not be redone.

Resume from the exact "Next exact task" in docs/AI_HANDOFF.md.

Do not restart the project.
Do not redesign completed work without evidence and a documented decision.
Do not move to another milestone.
Do not trust unverified claims merely because code exists.

Run appropriate existing tests before changing sensitive code.

Before ending, update docs/AI_HANDOFF.md so another agent with zero chat history can continue immediately.

Proceed autonomously with the current milestone.
```

## M0 execution prompt

```text
MILESTONE M0 — REPOSITORY BOOTSTRAP AND TECHNICAL SPECIFICATION

Work ONLY on M0. Do not implement the miner yet.

Purpose:
Establish a trustworthy technical foundation for an AMD XDNA1 NPU-accelerated Qubic miner.

Before starting, read AGENTS.md and every document referenced by docs/AI_HANDOFF.md. Inspect git status, current branch and recent commits.

PRIMARY OBJECTIVES

1. Research and document the CURRENT Qubic mining algorithm and protocol from authoritative upstream sources.
2. Determine exactly which computational workload is suitable for XDNA1 acceleration.
3. Separate CPU responsibilities from NPU responsibilities.
4. Define the CPU-golden-reference-first development strategy in concrete workload terms.
5. Audit upstream licenses before any code reuse.
6. Refine later milestone acceptance criteria if current upstream evidence requires it.

RESEARCH REQUIREMENTS

Determine from current authoritative Qubic sources:
- current mining algorithm;
- mining job/work lifecycle;
- data supplied to miners;
- candidate generation/mutation behavior;
- scoring process;
- relevant model/ANN/workload structure if applicable;
- relevant datatypes;
- vector/matrix/state dimensions;
- number and nature of repeated operations;
- memory-access patterns;
- validation requirements;
- result/share submission mechanism;
- pool/node communication requirements;
- protocol/version compatibility and algorithm-switch concerns.

Do not guess.

For every material protocol claim, record the authoritative source in docs/UPSTREAM.md including exact repository, URL, tag/commit/version and relevant source file/path where available.

LICENSING

Audit licenses of upstream reference implementations.

Explicitly document:
- what may be used as documentation/reference;
- what code may be reused;
- what code must NOT be copied;
- whether clean-room implementation is required;
- notices/attribution or other obligations.

ARCHITECTURE STUDY

Identify candidate XDNA1 kernels.

For each candidate operation estimate/document:
- arithmetic structure;
- integer/float datatype;
- overflow/saturation/rounding semantics;
- memory-access pattern;
- expected reuse/residency;
- batchability;
- CPU/NPU transfer requirements;
- expected AIE2 suitability;
- four-column mapping possibilities;
- correctness risks.

Use the current architecture as a hypothesis, not a fact:

Qubic node/pool
        |
        v
CPU network/job manager
        |
        v
CPU candidate/control
        |
        v
XDNA1 batched compute/scoring
        |
        v
CPU result verification
        |
        v
share/solution submission

Do not place work on the NPU merely to maximize an artificial NPU percentage.

M0 ACCEPTANCE CRITERIA

M0 is complete only if:
- authoritative current Qubic mining behavior is documented;
- upstream versions/licenses are recorded;
- CPU/NPU responsibility boundary is evidence-based;
- candidate NPU kernels are identified and ranked;
- arithmetic and memory semantics needed by M1 are sufficiently precise;
- later milestones have measurable acceptance criteria;
- unresolved protocol uncertainty is explicitly listed rather than guessed;
- docs/AI_HANDOFF.md contains a precise M1 starting task.

Do NOT start M1.

At the end:
- update docs/AI_HANDOFF.md;
- update docs/DECISIONS.md, docs/ARCHITECTURE.md, docs/TESTING.md, docs/UPSTREAM.md and docs/MILESTONES.md where evidence requires;
- provide a concise report of what changed, what was verified, what remains, current milestone status, and the exact next-agent recommendation.
```

## Emergency handoff prompt when an agent is near its context/usage limit

```text
Stop starting new implementation work.

Preserve the repository for a zero-context successor.

1. Finish or safely revert any half-written local change that cannot be left in a coherent state.
2. Run the most relevant quick tests that fit the remaining session.
3. Update docs/AI_HANDOFF.md with exact current state, including incomplete work and failures.
4. Record architectural decisions in docs/DECISIONS.md.
5. Record test commands/results in docs/TESTING.md or the handoff.
6. Record benchmark results only if actually measured.
7. State the next exact task at file/function/test level.
8. Do not claim the current milestone complete unless all acceptance criteria pass.

End with a concise handoff report. Do not begin another task.
```

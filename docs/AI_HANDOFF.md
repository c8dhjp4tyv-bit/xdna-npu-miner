# AI Handoff

This file is the authoritative short-form state for the next zero-context coding agent.

## Current milestone

**M0 — Repository bootstrap and technical specification**

## Status

**NOT STARTED**

Repository scaffolding and milestone governance were created, but M0 protocol/licensing research has not yet been executed.

## Branch

`main`

## Current commit

Update this at the end of each working session if available.

## Work completed

- Standalone repository initialized.
- Multi-agent operating rules added in `AGENTS.md`.
- Project goals and evidence standards added in `docs/PROJECT_SPEC.md`.
- M0–M11 milestone gates defined in `docs/MILESTONES.md`.
- Initial handoff structure created.

## Files changed / created

- `README.md`
- `AGENTS.md`
- `docs/PROJECT_SPEC.md`
- `docs/MILESTONES.md`
- `docs/AI_HANDOFF.md`

Additional scaffold documents may be created in the same bootstrap session before M0 begins.

## Tests executed

None. No implementation exists yet.

## Hardware tests actually executed

None.

## Known failures

None yet. No mining or NPU functionality has been implemented.

## Assumptions requiring M0 verification

- Qubic is the first intended target.
- A compute-heavy scoring/ANN-like portion of the current Qubic mining workload may be suitable for XDNA1 acceleration.
- CPU should likely retain networking/orchestration/control while XDNA1 handles repeated batchable compute.

These are design hypotheses, not yet accepted protocol facts. M0 must verify them against current authoritative upstream sources.

## Important architectural decisions

- Repository state, not chat history, is the project memory.
- CPU golden reference must precede mining NPU acceleration.
- Every NPU kernel requires differential correctness tests against CPU.
- No silent CPU fallback may be represented as NPU execution.
- Performance evidence must follow correctness evidence.
- Mining must be limited to hardware the operator owns or is authorized to use.

## Things the next agent must NOT redo

- Do not create a second project/repository.
- Do not discard the milestone system.
- Do not start implementation before completing M0 research.
- Do not assume remembered Qubic protocol details are current.
- Do not copy upstream miner code until licensing is audited.

## Next exact task

Execute **Milestone M0 only**.

1. Read all repository governance/specification documents.
2. Research the CURRENT Qubic mining algorithm and mining protocol from authoritative upstream Qubic sources.
3. Record exact repositories, URLs, tags/commits/versions and relevant source files in `docs/UPSTREAM.md`.
4. Document:
   - job/work lifecycle;
   - data received by miners;
   - candidate generation/mutation behavior;
   - scoring/validation behavior;
   - result/share submission;
   - relevant datatypes and dimensions;
   - repeated arithmetic operations;
   - memory-access pattern;
   - protocol/version compatibility.
5. Audit licenses of relevant upstream implementations and state what can be reused, what cannot, and whether clean-room implementation is required.
6. Identify and rank candidate XDNA1 kernels by arithmetic fit, batching potential, transfer cost, memory behavior and correctness risk.
7. Refine `docs/ARCHITECTURE.md`, `docs/TESTING.md`, `docs/UPSTREAM.md`, `docs/DECISIONS.md` and `docs/MILESTONES.md` if evidence requires changes.
8. Do **not** start M1.
9. Update this file with the precise M1 starting task only after all M0 acceptance criteria pass.

## Relevant commands for the next agent

```bash
git status -sb
git branch --show-current
git log --oneline -10
```

When source code eventually exists, run only milestone-relevant tests and record exact results here.

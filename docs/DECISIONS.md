# Engineering Decisions

Record material decisions here so later agents do not silently reverse them.

## D-001 — Standalone repository

**Status:** Accepted

The mining project is separate from `hawkpoint-npu-llm`.

Reason: mining has different protocol, security, benchmark, release and optimization concerns. XDNA1 knowledge may be reused conceptually, but repository state and dependencies remain independent.

## D-002 — Qubic is the first intended target, pending M0 verification

**Status:** Provisional

Qubic is the initial target because its current useful-work/mining computation may map better to XDNA1 than conventional memory-hard or ASIC-dominated PoW algorithms.

M0 must verify the current algorithm and workload before implementation. If authoritative current evidence contradicts the suitability hypothesis, M0 may recommend a different target with documented justification.

## D-003 — CPU golden reference precedes NPU mining kernels

**Status:** Accepted

No production mining compute path should be ported to XDNA1 until a deterministic CPU reference exists for the same arithmetic semantics.

## D-004 — No cosmetic NPU mode

**Status:** Accepted

NPU mode requires evidence of actual XDNA1 execution. Silent CPU fallback may exist only if explicitly designed and clearly reported as fallback; it must never be counted as NPU throughput.

## D-005 — Repository is the agent memory

**Status:** Accepted

AI agents may change frequently and may have zero prior conversation context. `AGENTS.md`, `docs/AI_HANDOFF.md` and milestone documents are authoritative.

## D-006 — Direct optimization claims require measured evidence

**Status:** Accepted

No speedup, energy-efficiency or profitability claim is accepted without controlled methodology and recorded results. Missing telemetry must be reported as missing rather than estimated.

## D-007 — Licensing is a gate, not cleanup work

**Status:** Accepted

Upstream reference miner/protocol code must be license-audited during M0 before reuse. Clean-room reimplementation is preferred when reuse rights are unclear or incompatible.

## Decision template

Append future decisions using:

```text
## D-NNN — Title

Status: Proposed / Provisional / Accepted / Superseded
Date:
Milestone:

Decision:

Evidence / rationale:

Alternatives considered:

Consequences:

Supersedes / superseded by:
```

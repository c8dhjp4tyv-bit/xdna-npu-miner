# XDNA NPU Miner

Experimental AMD XDNA1 NPU-accelerated cryptocurrency mining research project.

The first target is **Qubic (QUBIC)**, subject to Milestone M0 validating the current upstream mining algorithm, protocol, licensing constraints, and the exact compute kernels suitable for AMD XDNA1 acceleration.

## Target platform

- AMD Ryzen AI Hawk Point
- XDNA1 / `RyzenAI-npu1`
- AIE2, 4-column array
- Fedora Linux as the primary development platform
- MLIR-AIE / IRON / XRT-based native NPU execution where appropriate

## Engineering principles

1. Correctness before performance.
2. Every NPU kernel must have a trusted CPU golden reference.
3. Never claim NPU execution without hardware evidence.
4. Never invent benchmark results.
5. Keep CPU control/network work separate from compute kernels that genuinely benefit from XDNA1.
6. The repository is the persistent memory for all AI coding agents.
7. Agents must read `AGENTS.md` and `docs/AI_HANDOFF.md` before making changes.
8. Milestones advance only after their acceptance criteria pass.

## Current status

**Milestone M0 — Repository bootstrap and technical specification**

Mining functionality is not implemented yet. M0 exists to establish authoritative protocol knowledge, licensing boundaries, architecture, tests, and measurable milestone acceptance criteria before implementation begins.

## Repository memory

The project is intentionally designed for frequent handoffs between AI coding agents with limited context windows.

Start here:

1. `AGENTS.md`
2. `docs/AI_HANDOFF.md`
3. `docs/PROJECT_SPEC.md`
4. `docs/MILESTONES.md`
5. `docs/ARCHITECTURE.md`
6. `docs/DECISIONS.md`
7. `docs/TESTING.md`
8. `docs/UPSTREAM.md`

## Safety and scope

This repository is for legitimate cryptocurrency mining research on hardware the operator is authorized to use. It must not add hidden persistence, unauthorized resource use, credential theft, propagation, or stealth-mining behavior.

## License

Project license has not yet been finalized. M0 must audit upstream licenses before any reusable implementation is imported or adapted.

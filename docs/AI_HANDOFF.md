# AI Handoff

This file is the authoritative short-form state for the next zero-context
engineering agent.

## Current milestone

**M1 — CPU golden reference**

## Status

**NOT STARTED**

Direct-node algorithm, task, epoch/seed/threshold, scoring, validation,
licensing, CPU/NPU boundary, correctness contract, and benchmark method are
documented. M0 is **COMPLETE**. Official Qubic sources conflict about Qatum's
status, but the direct Qubic-node path is canonical and the conflict is not an
M0 blocker. M1 through M5 have zero dependency on Qatum or any mining pool;
M6 starts with direct-node integration. Do not invent a Qatum wire contract.

M0 contains no mining, network, CPU scorer, NPU runtime, or kernel
implementation.

## Branch and commits

- Branch: `main`
- Bootstrap baseline before M0 edits:
  `b2fecb0ffa7761a9099f984cf8d3d51d458b06c`
- M0 cleanup commit: current `HEAD`; use `git rev-parse HEAD` for its exact
  SHA (the final session report records it).

## Exact work completed

- Recovered the already bootstrapped repository; it was not recreated.
- Read, in the requested order, `AGENTS.md`, all M0 governance/specification
  documents, `docs/AGENT_PROMPTS.md`, and `README.md` before editing.
- Inspected branch, status, directory placeholders, and the last 15 commits.
- Pinned current active Qubic BPP9000 to core
  `v1.301.3 / a83f935406cd006b5b1a94971139e74d410ecb6d`.
- Pinned current Qiner reference miner to
  `v1.302.3 / 11fb18a6f4944bb55fe103d3f263cb5d31e00200`.
- Pinned QLI client documentation to
  `v3.7.2 / 9a01902342240c69b19d9cceb637ea68916a3d2c`.
- Inspected official Qubic mining/pool/FAQ documentation and recorded that
  official sources conflict: the current FAQ says Qatum is in development,
  while an older official blog says it launched and the ecosystem entry labels
  it live.
- Inspected the related Hawk Point XDNA1 repository at
  `15f3352779e944ccd202b2e166e40e197a1de759` and MLIR-AIE at
  `57d7494e99c214f5f53b328a0ed43a99e759e835` as environment references only.
- Recorded exact BPP9000 dimensions, types, task layout, random2, recurrent
  tick, mutation, score, timeout, threshold, direct-node framing, and
  solution-validation behavior.
- Recorded the Qiner/core example-task and threshold discrepancy.
- Completed the Anti-Military/no-SPDX and missing-license audit and selected
  clean-room reimplementation.
- Ranked K1 recurrent tick, K2 fused score, K3 fused search, K4 reduction, and
  K5 random2/K12 for XDNA1 suitability without performance claims.
- Designed the M1 scalar CPU reference and exact CPU/NPU correctness contract.
- Expanded all M1–M11 gates and benchmark methodology.

## Files changed

- `docs/PROJECT_SPEC.md`
- `docs/ARCHITECTURE.md`
- `docs/MILESTONES.md`
- `docs/UPSTREAM.md`
- `docs/DECISIONS.md`
- `docs/TESTING.md`
- `docs/BENCHMARKS.md`
- `docs/AI_HANDOFF.md`
- README was not changed; its statement that no mining implementation exists
  remains accurate.

No files under `src/`, `tests/`, `benchmarks/`, or `scripts/` were implemented.

## Sources/revisions inspected

- Qubic core: tag `v1.301.3`,
  `a83f935406cd006b5b1a94971139e74d410ecb6d`.
- Qiner: tag `v1.302.3`,
  `11fb18a6f4944bb55fe103d3f263cb5d31e00200`.
- Official docs: mining, software, pool, FAQ, Qatum blog, and ecosystem entry,
  accessed 2026-08-09.
- QLI client: tag `v3.7.2`,
  `9a01902342240c69b19d9cceb637ea68916a3d2c`.
- Hawk Point reference: branch `revert/kernel-pin-441`,
  `15f3352779e944ccd202b2e166e40e197a1de759`.
- MLIR-AIE: `57d7494e99c214f5f53b328a0ed43a99e759e835`.

See `docs/UPSTREAM.md` for repository URLs, paths, license details, task
SHA-256 values, and reproducibility commands.

## Checks executed

During source audit:

- clean target-repository status and branch/log inspection;
- exact source revision and license-file inspection;
- canonical and Qiner example task size/SHA-256 comparison;
- source-path searches for algorithm, system-info, framing, broadcast,
  transaction, and pool configuration facts.

Document checks completed before the commit attempt:

- `git diff --check` passed with no whitespace errors;
- `pandoc --from=gfm --to=html --output=/dev/null` parsed all eight modified
  Markdown documents successfully;
- Qatum scope scan found no guessed wire schema, M1 implementation, or
  measured NPU performance claim;
- canonical task SHA-256:
  `0c5e9e42c6d86c320af62f4125ca85b2446f2b098893fd6521bcf66c22f7f00a`;
- Qiner example task SHA-256:
  `403e24225f5b0512d0cbf49758fed9a01e7334d3cea565ad6c5e82420b713226`.

Hardware tests actually executed: none. No XDNA1 device, runtime, kernel, or
live Qubic node was exercised during M0.

The final closing checks are:

```bash
git status -sb
git diff --check
git branch --show-current
git log --oneline -15
git rev-parse HEAD
```

There is no build system or executable test command in the bootstrap
repository; M0 is a document/source audit. Do not claim a build or test pass.

## Known uncertainties

1. Qatum pool wire protocol, version negotiation, job/share schema, task
   distribution, authentication details, and reconnect behavior are not
   sufficiently complete and consistently pinned in authoritative sources.
   This is a deferred optional adapter gate, not an M0 or M1–M5 blocker.
2. The QLI client documents one WebSocket/JWT service, but its checked-in
   documentation is not a canonical interoperable protocol and its repository
   has no license file.
3. The full task file must be obtained through an authorized/current
   distribution and hash-checked; core source pins the expected production
   hashes, but M0 did not connect to a live node.
4. The independent K12/signature implementation and its target-project license
   must be selected in M1/M6; do not copy Qubic crypto code.
5. XDNA1 tile capacity, exact compiler/runtime compatibility, and useful batch
   size are empirical M2–M5 questions. No NPU or performance result exists.

## Licensing constraints

Qubic core and Qiner source use a custom Anti-Military License without an SPDX
identifier. The narrow core MIT exception covers only the listed uint128 files.
QLI client has no detected license. Treat all Qubic/QLI implementation source
as reference-only and reimplement clean-room. Do not copy source structure or
code into this repository. If any upstream code is proposed for reuse, stop
for file-level license/legal review and preserve required notices/restrictions.

## Important decisions

- BPP9000 (`nonce[0] == 1`) is the current target; Neuraxon is reserved.
- Core production task/hash and runtime threshold outrank Qiner example values.
- Runtime system info supplies current seed/threshold; do not hard-code 6,469.
- CPU owns networking, freshness, task validation, candidate control,
  mutation/rollback, crypto/signing, and final canonical verification.
- First XDNA hypothesis is candidate/window-batched recurrent LUT ticks; four
  columns initially partition complete independent work, not tick stages.
- Exact equality is required; `2` is UNKNOWN, not `-1`; no tolerance or hidden
  saturation is allowed.
- No benchmark claim is allowed before correctness and dispatch evidence.
- Direct-node support is the canonical first network target; Qatum/pool support
  is optional, version-gated, and currently unresolved.

## Things the next agent MUST NOT redo

- Do not recreate the repository or replace the scaffold.
- Do not repeat the completed source/license audit unless upstream revisions
  changed; start from `docs/UPSTREAM.md` and its pinned SHAs.
- Do not copy Qubic core/Qiner/QLI source.
- Do not implement networking, pool support, NPU kernels, batching, or M2.
- Do not use Qiner's example task or 6,469 threshold as production truth.
- Do not report static operation counts as measured performance.
- Do not treat Qatum uncertainty as an M0 blocker or invent its wire protocol.
- Do not implement Qatum/pool integration until direct-node support works and a
  sufficiently complete authoritative specification or implementation is pinned
  and independently reviewed.
- Do not submit any future accelerated result without CPU canonical
  verification.

## Next exact task

Begin **M1 — CPU golden reference**, and only M1:

1. Define fixed-width domain types and a readable task parser for the pinned
   BPP9000 file format.
2. Implement a scalar, double-buffered recurrent BPP9000 reference with
   injectable deterministic random2/K12 boundaries, mutation, accept/rollback,
   score/timeout, threshold predicates, and a pure reference score API.
3. Add deterministic golden and edge-case vectors plus the required correctness
   matrix in `docs/TESTING.md`, with production-shaped metadata/vector
   provenance.
4. Cross-check behavior against the pinned core reference/Qiner behavior
   without copying source.
5. Add the reproducible M1 build/test command and update this handoff.

M1 must not include AVX optimization, NPU code, MLIR-AIE, IRON, XRT, Qatum,
pool integration, direct-node networking, or performance optimization.

Stop after M1. Do not begin M2, XDNA runtime, speculative AIE2 kernels,
network/pool integration, or optimization.

## Relevant commands

```bash
cd /home/umutcagand/xdna-npu-miner
git status -sb
git branch --show-current
git log --oneline -15
git diff --check
```

For the source audit, see the clone/checkout/hash commands in
`docs/UPSTREAM.md`. The target repository must remain standalone.

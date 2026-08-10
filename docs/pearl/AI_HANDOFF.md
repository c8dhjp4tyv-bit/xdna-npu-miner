# Pearl (PRL) AI Handoff

This is the authoritative handoff for the Pearl research track. The
repository-level handoff also records the frozen Qubic track; do not resume
Qubic M6/M7 or Qatum work while this track is active.

## Current milestone

**P0 — upstream/spec/license/kernel/protocol/XDNA1 feasibility baseline**

## Status

**COMPLETE — documentation and evidence only.**

## Branch and commit

- Branch: `feat/pearl-m0`
- P0 commit: pending at the documentation checkpoint; record the exact SHA
  after commit before handoff.
- Remote must be verified to contain the final P0 commit before this task is
  reported complete.

## Completed work

- Recovered and pushed the two valid pre-existing Qubic M6 documentation
  checkpoints before branching; Qubic is now frozen/reference-only.
- Pinned Pearl `master` at
  `fe22b6a2b831d95b2f56564808f39d2f498f34a5` and the unversioned official
  whitepaper by SHA-256.
- Traced the node → gateway → job → matrix → proof → submission path.
- Verified the current dense kernel's `int8 x int8 -> int32` main multiply,
  `r=128` miner settings, `128 x 256 x 128` CUDA tile, `2 x 64` inner pattern,
  keyed BLAKE3 transcript, noise, Merkle openings, and CPU/Rust ZK proof path.
- Reviewed component licenses and adopted a clean-room-only rule for the
  unclear hot miner/proof/gateway components.
- Compared the dense primitive with official XDNA1/AIE2 documentation and set
  the single gate to `UNKNOWN_NEEDS_EXPERIMENT` with primitive assessment
  `POSSIBLE_FIT`.
- Added no Pearl source implementation and made no Qubic source/Qatum/identity
  changes.

## Files changed by this track

- `docs/pearl/PROJECT_SPEC.md`
- `docs/pearl/ARCHITECTURE.md`
- `docs/pearl/UPSTREAM.md`
- `docs/pearl/MILESTONES.md`
- `docs/pearl/AI_HANDOFF.md`
- `docs/evidence/pearl-p0.json`
- top-level handoff/spec/architecture/decisions/testing/milestones/upstream/
  benchmark pointers documenting the separate Pearl track

## Verification

P0 checks to run before commit:

```bash
python3 -m json.tool docs/evidence/pearl-p0.json >/dev/null
git diff --check
git status --short --branch
```

Results before commit: JSON parsing PASS; independent arithmetic checks PASS;
`git diff --check` PASS; `cmake --build build -j2` PASS; and
`ctest --test-dir build --output-on-failure` PASS with 6/6 tests. The worktree
was clean before the Pearl edits and is intentionally dirty only with the P0
documentation until the commit below is created. The final commit SHA and
remote verification must be filled in after commit/push.

## Hardware tests actually executed

No Pearl kernel, Pearl CPU golden path, Pearl job parser, live node, pool, or
submission was executed in P0. Existing physical Qubic M5 evidence confirms
that this host has `RyzenAI-npu1`/AIE2 and a measured four-column runtime, but
it is not Pearl evidence and must not be relabeled.

## Known blockers and assumptions

- Pearl hot miner/proof/gateway component reuse licenses are unresolved;
  implement clean-room code or obtain a legal review before copying anything.
- Whitepaper input-range language and current quantization source require a P1
  canonical-vector decision.
- No official external pool transport/protocol was found.
- XDNA1 int8 GEMM support is plausible, but Pearl-specific tiling, noise
  correction, transcript hashing, transfer cost, and proof overhead are
  unmeasured.
- No minimum useful batch size, four-column benefit, hashrate, or profitability
  is known.

## Architectural decisions

- Pearl is a separate `docs/pearl/` research track; no P0 `src/pearl/` code.
- CPU owns network, freshness, control, proof, verification, and submission.
- XDNA1 may receive only explicit Pearl buffers and returns untrusted compute
  results for CPU comparison.
- `UNKNOWN_NEEDS_EXPERIMENT` is the only P0 overall gate; it cannot be upgraded
  from documentation alone.

## Exact next task

Implement P1's clean-room CPU golden path only after the P0 commit is pushed.
Start with a fixed, versioned header/config serializer and independently
checked vectors for commitments, noise, int32 noised products, the `2 x 64`
selected transcript, target comparison, and `PlainProof` fields. Do not begin
P2 or connect to a node/pool until those vectors are complete.

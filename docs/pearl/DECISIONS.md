# Pearl One-Shot Decisions

- The AIE2 P2 kernel is project-owned and uses canonical IRON lane transforms;
  a direct row-major stream was rejected after sparse differential mismatches.
- CPU remains authoritative for noise, correction, transcript, BLAKE3, target,
  openings, PlainProof, freshness, and submission policy. NPU results are
  discarded on any mismatch and no CPU fallback is labeled as NPU execution.
- P8 selected four columns and batch eight only from a fixed physical sweep;
  the measured result, not the requested column count, controls the default
  recommendation.
- Unclear-license Pearl hot components and the useful-work/prover runtime are
  external boundaries. No source copy or fabricated live tensor provider is
  permitted.

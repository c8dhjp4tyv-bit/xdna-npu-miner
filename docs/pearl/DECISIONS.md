# Pearl One-Shot Decisions

- P7 uses a dedicated official-wire adapter. The project-owned P1 PlainProof
  envelope remains for local evidence, while the pinned gateway receives the
  official Rust/bincode field order and `Option::None` dense-proof tag. The
  two encodings must not be conflated.
- P7's official SIMNET proof uses deterministic dense matrix fixtures to
  exercise the real physical-XDNA → CPU verification → official prover → node
  path. Consensus validates commitments, openings, noise, transcript, and
  target; it does not expose a model-identity signature. This is an
  interoperability proof, not a claim to have reproduced the official
  CUDA/vLLM useful-work source or a mainnet mining strategy.
- Raw Pearl mining signals are inclusive `[-64,64]`. Noised signed-int8
  operands are allowed to leave that raw-source interval, provided they fit
  the physical signed-int8 input contract; validating noised operands against
  the raw bound would reject official-compatible proofs.
- The P7 SIMNET acceptance was made with a CPU-only Torch environment and no
  vLLM/CUDA miner. Mainnet payout configuration and pool/Stratum support
  remain separate unavailable boundaries.
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

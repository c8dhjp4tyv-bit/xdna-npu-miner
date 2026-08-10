# Pearl P1 canonical corpus

This corpus is the fixed CPU-golden input/output record for Pearl P1. It is
clean-room data, not a copy of a Pearl fixture. The implementation under test
is `src/pearl/reference.cpp`; the Rust helper is the separately pinned
`blake3` 1.8.2 dependency described in `docs/pearl/UPSTREAM.md`.

The corpus is intentionally split into semantic records rather than one
opaque proof blob. Every record has explicit widths, little-endian rules, and
an expected digest/value. `PlainProof` round-trip identity and malformed
input rejection are exercised by `pearl_cpu_golden_tests`.

| ID | Coverage |
|---|---|
| A | signed int8×int8→int32 arithmetic and checked overflow |
| B | signed signal boundaries `[-64,63]` and noise boundaries `[-64,64]` |
| C | fp32 scale, zero-point absence, ties-to-even, and `[-63,63]` quantization |
| D | deterministic uniform/sparse noise, labels, rank, and seed derivation |
| E | header/config/pattern serialization and current/structural validation |
| F | job key, matrix roots, and chained commitment/noise seeds |
| G | selected `2×64` data and packed/full selection layouts |
| H | 16-word transcript trace and rotate-left-13 updates |
| I | keyed jackpot digest and little-endian `<=` target comparison |
| J | 1024-byte Merkle leaves, selected rows, sibling order, and tamper failure |
| K | fixed-width P1 PlainProof envelope and round-trip identity |
| L | truncated, oversized, noncanonical, out-of-range, and overflow failures |

The test binary also runs 24 deterministic seeded randomized cases across
rank 32/64/128, valid `k`, edge signal values, alternate selected columns,
noise seeds, and targets. No benchmark or NPU execution is implied by this
corpus.

# Pearl (PRL) XDNA Miner — Full Project Specification

## State and scope

P2–P6, P8, P9, and the P11 delivery implementation are now present in the
one-shot branch. P10 is the current physical endurance gate. P7 remains
`BLOCKED` until an official gateway/prover/useful-work provider and a
local/simnet node are available. The aggregate must therefore remain
`SOFTWARE_COMPLETE_E2E_BLOCKED` unless that external interoperability is
actually proven.

This is the active Pearl implementation track on branch
`feat/pearl-full-miner-one-shot`, created from P1 checkpoint
`ba286d5770c93290a38784f89ae75cea87867b25`. P0 established the pinned
protocol/license baseline and P1 established the trusted clean-room CPU golden
reference. P2 through P11 are now executed in one continuous engineering shot
with separate, sequential evidence gates.

The completed Qubic work remains the frozen reference track. Its source,
Qatum boundary, identities, and M6/M7 state are not being changed by Pearl
research. Pearl-specific architecture and roadmap are kept under
`docs/pearl/` so the two protocols cannot be confused.

## Research question

Can Pearl's current proof-of-useful-work matrix-multiplication hot path be
implemented as a correct, measurable XDNA1/AIE2 backend on AMD Hawk Point, with
the CPU retaining protocol, proof, and submission authority?

The P0 answer is deliberately conservative:

```text
dense int8 MAC/reduction primitive: POSSIBLE_FIT
overall Pearl mining path: UNKNOWN_NEEDS_EXPERIMENT
```

The primitive is a plausible AIE2 workload, but no Pearl kernel has yet been
implemented or differentially verified on this host. P0 makes no hashrate,
latency, energy, profitability, or speedup claim.

## Execution status and gate

P1 is **COMPLETE** because the focused CPU test target, the full existing
CTest suite, JSON evidence, corpus digest, and `git diff --check` all pass on
the same clean-room implementation. The P1 implementation is intentionally
separate from `src/bpp9000/`, `src/qubic/`, and all Qubic wire types.

P2 is **IN PROGRESS** and owns only the physical signed-int8 GEMM gate. Later
milestones may be prepared only after their predecessor passes or is truthfully
recorded as an external blocker; a later pass cannot erase an earlier failure.

The current CPU reference is in [`../../src/pearl/reference.hpp`](../../src/pearl/reference.hpp)
and [`../../src/pearl/reference.cpp`](../../src/pearl/reference.cpp). The
canonical corpus is in [`../../tests/data/pearl/p1/vectors.json`](../../tests/data/pearl/p1/vectors.json).
The CMake target is `pearl_cpu_golden_tests`; it links a small Rust FFI helper
whose only cryptographic dependency is the official `blake3` crate pinned to
`1.8.2` and licensed `CC0-1.0 OR Apache-2.0`.

## P1 resolved CPU contracts

- Raw mining matrices are signed int8 values in `[-64,63]`. The current
  quantizer uses fp32 `scale=max_abs/63`, no zero point, ties-to-even rounding,
  and clamps quantized output to `[-63,63]`. Thus the whitepaper's `+64` is not
  accepted by the pinned current raw mining path; `-64` remains a valid raw
  boundary value. This distinction is recorded in the vectors rather than
  silently conflated.
- Every dot product widens signed int8 operands to int64 for accumulation and
  rejects a result outside int32. No wrapping or saturation is used.
- Header fields, patterns, dense configuration, public data, openings,
  transcript words, and P1 PlainProof fields use explicit little-endian
  widths. Header hash fields use the pinned 76-byte reversed wire order.
- Noise uses the pinned `A_tensor`/`B_tensor` labels, keyed BLAKE3-derived
  uniform bytes in `[-32,31]`, sparse `+1/-1` permutation pairs, rank-dependent
  factors, `noise_range=128`, and `idxs_per_col=2`.
- The selected data is the current `[0,8]` by `[8j,8j+1]` (`j=0..31`)
  `2x64` tile. The implementation supports both its compact selected layout
  and the corresponding full-matrix coordinates. Each full-r cumulative
  product is XOR-reduced as int32 bit patterns and applied to transcript slot
  `reduction_index mod 16` with rotate-left 13.
- Jackpot hashing is keyed BLAKE3 over 16 little-endian u32 words, interpreted
  as a little-endian 256-bit integer with the pinned `<=` target rule.
- Merkle leaves are 1024 bytes. Openings are sorted, unique selected rows with
  canonical sibling ordering and exact root verification. P1's `PlainProof`
  is a fixed-width candidate envelope immediately before CPU/Rust proof
  generation; it is not a ZK certificate and no ZK proof is generated.

The full contract, fixed values, negative cases, upstream black-box result,
and seeded randomized count are recorded in
[`../../docs/evidence/pearl-p1.json`](../evidence/pearl-p1.json).

## Authoritative Pearl revision

All facts in this document are pinned to the following primary sources as
observed on 2026-08-10:

| Role | Pin | Notes |
|---|---|---|
| Pearl monorepo | `master` at `fe22b6a2b831d95b2f56564808f39d2f498f34a5` | Current `HEAD`; commit `chore: add coinmarketcap.txt for listing verification (#277)` |
| Mining implementation revision | `fe22b6a2b831d95b2f56564808f39d2f498f34a5` | `miner/`, `zk-pow/`, `pearl-blake3/`, and `py-pearl-mining/` at the same tree |
| Node/protocol revision | `fe22b6a2b831d95b2f56564808f39d2f498f34a5` | `node/`, `node/btcjson/`, and `node/chaincfg/` at the same tree |
| Application/network version | `1.3.1` | `version/version.go` constants at the pinned tree |
| Node JSON-RPC API semver | `1.3.0` | `node/rpcserver.go` API constants; distinct from the application version |
| Research/whitepaper revision | Unversioned PDF, SHA-256 `0b7dc4f064a926c4e8b6dfb8de12fe5cf041d713d8c0426f983f2833da5b8f3c` | 25-page `Pearl_Whitepaper.pdf`; no explicit revision string found |

Primary URLs:

- Repository: <https://github.com/pearl-research-labs/pearl>
- Official site: <https://pearlresearch.ai/>
- Whitepaper: <https://pearlresearch.ai/Pearl_Whitepaper.pdf>
- Compute/mining site: <https://compute.pearlresearch.ai/>

The source-file map, exact URLs, license observations, and reproducibility
commands are in [`UPSTREAM.md`](UPSTREAM.md).

## Verified dense mining computation

The dense path treats the useful-work matrices as `A[m,k]` and `B[k,n]`.
The implementation stores the second operand as `B^T[n,k]` for selected-column
opening and hashing. A candidate performs a noised multiplication, then the
proof path removes the low-rank noise and authenticates selected output data.

The current official GPU settings and consensus API identify these important
parameters:

| Parameter | Pinned current value or rule |
|---|---|
| Consensus MMA enum | `Int7xInt7ToInt32` (enum value `0`) |
| Current consensus rank floor | `r >= 128` after the rank-penalty rule |
| Rank domain | Power of two, `32 <= r <= 1024`, and r is divisible by 16 |
| Common dimension `k` | `k <= 2^16`, k is divisible by 64, `k >= 1024`, and `16r <= k <= 4r^2` |
| Dot-product length | `k - (k mod r)`; it must be divisible by the circuit dword size |
| Inner pattern bounds | `h` and `w` are divisible by 2, with `32 <= h*w <= 256` |
| Matrix/worker bounds | `m,n <= 2^24`; `(h+w)*dot_product_length <= 2^22` bytes |
| Current GPU noise rank | `128` |
| Current GPU tile | `128 x 256 x 128` (`M x N x K`) |
| Inner hash rows | `[0, 8]`, so `h = 2` |
| Inner hash columns | `[8j, 8j+1]` for `j=0..31`, so `w = 64` |
| Inner hash tile | `2 x 64 = 128` selected values |
| Tile depth | `TILE_D = 16` |
| Jackpot transcript | 16 little-endian `u32` words, rotate-left by 13 after each XOR |
| Difficulty comparison | Little-endian 256-bit keyed BLAKE3 result compared with the derived target |

The `2 x 64` pattern is important: it is not the often-assumed `2 x 32`
pattern. The official current miner's pattern contains 64 column indices.

The current official miner is a CUDA/vLLM implementation targeting Hopper
`sm90` GPUs (the README names H100/H200). That is a useful upstream GPU
comparison point, not a compatible backend or a measured baseline for this
host.

### Precision and quantization

- The logical consensus name is W7A7-like (`Int7xInt7ToInt32`), but current
  code represents values in signed `int8` containers. P0 found no packed-int7
  instruction or bit-level packed representation in the current mining path.
- The main dense multiply is signed `int8 x int8 -> int32` accumulation. AIE2
  work would therefore target an int8 vector/MAC path with an explicit int32
  accumulation contract, not assume a native W7A7 instruction.
- Current vLLM quantization uses a symmetric range with `max_val=63`,
  round-to-nearest (`rint_f32`), and clamping to `[-63, 63]`; it carries an
  fp32 scale and no zero point. The current Python interface documents matrix
  entries in the signed int8 range, while the whitepaper describes a broader
  `[-64, 64]` input convention and `[-63, 63]` noise. This source-level
  difference must be resolved by P1 canonical vectors rather than guessed.
- Noise generation uses byte-derived values in `[-32, 31]` for uniform factors
  and sparse `+1/-1` factors. The current implementation uses `noise_range=128`
  and `idxs_per_col=2`.
- The CUDA GEMM's main accumulator is `int32`. The current denoising path can
  use fp16/fp32 intermediate correction and stores the final `C` as `bfloat16`
  with fp32 row scales. This means the current GPU implementation is not an
  end-to-end pure integer-output kernel even though its main PoUW product is
  integer MAC/accumulate.
- P0 found no basis to claim saturating or wrapping arithmetic as consensus
  behavior. P1 must define overflow, rounding, clamp, and layout behavior with
  trusted CPU vectors and reject any mismatch.
- The hot GEMM has no activation function or unrelated inference fusion. Hash,
  noise correction, and proof work remain separate correctness concerns.

### Commitment, noise, and proof dependency

The pinned path is not a generic hash loop:

1. Build a job key from the incomplete block header and serialized mining
   configuration.
2. Commit to padded `A` and `B^T` with keyed BLAKE3.
3. Derive `B` and `A` noise seeds and generate low-rank/sparse noise.
4. Multiply the noised tiles with an int32 accumulator.
5. For each full `r` chunk, XOR the selected tile values as `u32`, rotate the
   16-word transcript, and run the keyed BLAKE3 jackpot check.
6. On a hit, open selected rows/columns with Merkle data and create the
   certificate proof through the current `zk-pow` Plonky2/STARKy path.

The pinned certificate serializer has a V1 dense form of
`version(4) | header_hash(32) | public_data(164) | proof_data_len(4) | proof`
and a V2 form with explicit public-data length. The current proof blob limit is
60,000 bytes. Certificate version is carried by the node template's
`requiredcertversion`; it must not be inferred from a miner setting.

The proof is therefore dependent on exact matrix values, noise derivation,
selected rows and columns, commitments, inner transcript bytes, the incomplete
header, rank/configuration serialization, target comparison, and the ZK
certificate format. An accelerated GEMM that only produces numerically close
values is not sufficient.

## Network and mining boundary

The verified direct-node path is Bitcoin/BIP22-style HTTP JSON-RPC:

```text
Pearl node HTTP JSON-RPC
  getblocktemplate(capabilities, rules)
        -> gateway builds coinbase/merkle/header and caches MiningJob
        -> miner computes matrix candidate
        -> submitPlainProof over the local gateway transport
        -> gateway generates certificate/ZK proof
  submitblock(block_hex)
```

The gateway-to-node calls use `getblocktemplate` and `submitblock` over HTTP(S)
JSON-RPC with BasicAuth. The local miner-to-gateway interface is a
line-delimited JSON-RPC socket by default at `/tmp/pearlgw.sock`, or loopback
TCP port 8337 when configured, with `getMiningInfo` and `submitPlainProof`.
The pinned node README documents mainnet RPC port 44107 and P2P port 44108
(testnet/testnet2/simnet have separate configured ports).

No official Stratum server/client or external pool wire protocol was found in
the pinned monorepo. A `nbits_override` share-target hook exists in the proof
library, but it does not establish a pool transport. P0 records pool support
as unknown and does not implement or infer it.

## CPU/NPU responsibility contract

The eventual safe boundary is:

```text
Pearl node or separately verified pool
        -> CPU job/template manager
        -> CPU canonical preprocessing, commitments, and nonce/control policy
        -> XDNA1 candidate dense/noise GEMM and selected reduction (candidate)
        -> CPU exact transcript/proof verification
        -> CPU gateway/ZK generation and block/share submission
```

The CPU remains authoritative for freshness, serialization, noise/commitment
inputs, selected-row/column opening, exact verification, certificate creation,
and submission. An NPU result can never be submitted without an independent
CPU check. A CPU fallback must be explicit and must not be labeled NPU
execution.

## Conservative first-order feasibility model

For one representative official GPU output tile and one `r=128` chunk:

| Quantity | Model |
|---|---:|
| Multiply-accumulate pairs | `128 x 256 x 128 = 4,194,304` |
| Scalar integer operations, counting multiply and add separately | `8,388,608` |
| A/B operand bytes, one byte per signed int8 value | `49,152` |
| bf16 C bytes if materialized once | `65,536` |
| Lower-bound visible bytes in this simplified model | `114,688` |
| Arithmetic intensity under this convention | `73.14 scalar ops/byte` |

If an int32 output tile is materialized instead of bf16, the same simplified
model is `46.55 scalar ops/byte`. These are arithmetic/traffic estimates, not
measurements; real XDNA cost depends on L1 tiling, DMA reuse, denoising,
transcript reduction, BLAKE3 work, synchronization, and host transfers.

For the representative `k=2048, r=128` configuration there are 16 rank
chunks. The selected `2 x 64` proof pattern contains 128 values, or `1/256` of
the `128 x 256` output tile's positions. That ratio is a warning that useful
proof entropy and total GEMM work are not identical metrics. The minimum
effective batch size is not known; P2 must measure batch 1 for correctness and
then independent jobs at increasing batch sizes before choosing a batch.

Likely first-order bottlenecks are host/NPU movement, per-job commitment/noise
preparation, the selected-value XOR/rotate/BLAKE3 path, CPU Merkle openings,
and CPU/Rust ZK proof generation—not just the dense MAC rate. No Pearl or GPU
hashrate is claimed.

## P0 non-goals

- No `src/pearl/` implementation in P0.
- No wallet, live mining, share/block submission, pool adapter, supervisor,
  cloud-GPU path, or profit optimization.
- No unreviewed Pearl binary or source-code copy.
- No Qubic source, Qatum code, identity, or completed Qubic evidence changes.
- No claim that four XDNA1 columns improve Pearl throughput until measured.

The staged Pearl work and acceptance gates are in [`MILESTONES.md`](MILESTONES.md).

## P1 non-goals

- No XDNA1/AIE2 kernel, `/dev/accel` access, device dispatch, telemetry, or
  benchmark.
- No live Pearl node, gateway, pool, wallet, job parser, share/block
  submission, persistence, or profitability path.
- No ZK proving, certificate generation, or claim that a `PlainProof` is
  submit-ready.
- No reuse of unclear-license Pearl miner, gateway, proof, binding, or local
  BLAKE3 source.

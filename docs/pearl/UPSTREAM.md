# Pearl (PRL) P0 Upstream, Protocol, and License Record

## Source pins

Audit date: **2026-08-10**. Only the official Pearl repository and official
Pearl website/whitepaper were used for Pearl facts.

| Source | Exact revision | Relevant role |
|---|---|---|
| `pearl-research-labs/pearl` | `master` / `fe22b6a2b831d95b2f56564808f39d2f498f34a5` | Node, gateway, miner, proof, bindings |
| Official whitepaper | `Pearl_Whitepaper.pdf`, SHA-256 `0b7dc4f064a926c4e8b6dfb8de12fe5cf041d713d8c0426f983f2833da5b8f3c` | Research/specification reference; no embedded version/revision string found |
| CUTLASS gitlink | `291300ffffa3533a78ee104f08a8490a29ce9ccb` | Third-party submodule declared by `miner/pearl-gemm`; not copied |

URLs:

- <https://github.com/pearl-research-labs/pearl/tree/fe22b6a2b831d95b2f56564808f39d2f498f34a5>
- <https://pearlresearch.ai/>
- <https://pearlresearch.ai/Pearl_Whitepaper.pdf>
- <https://compute.pearlresearch.ai/>

The current application version is `1.3.1` from `version/version.go`. The
node's JSON-RPC API semver constants are `1.3.0` in `node/rpcserver.go`.
These are both recorded because a miner must not conflate binary version and
RPC API version.

## Mining-path source map

The following paths were read at the pinned commit; no source was copied into
this repository:

| Pinned path | Verified fact |
|---|---|
| `miner/pearl-gateway/src/pearl_gateway/pearl_client.py` | HTTP(S) JSON-RPC `getblocktemplate` and `submitblock` client with BasicAuth |
| `miner/pearl-gateway/src/pearl_gateway/scheduler.py` | Template refresh/cache loop; default refresh interval is one second |
| `miner/pearl-gateway/src/pearl_gateway/comm/dataclasses.py` | Coinbase, Merkle root, incomplete header, target, certificate version, and `MiningJob` fields |
| `miner/pearl-gateway/src/pearl_gateway/miner_rpc/server.py` | Local line-delimited JSON-RPC; `getMiningInfo` and `submitPlainProof` |
| `miner/pearl-gateway/src/pearl_gateway/submission_service.py` | Stale-job and certificate checks, proof generation, and final `submitblock` |
| `miner/miner-base/src/miner_base/async_loop_manager.py` | Polls mining info and sends a `PlainProof` after a GPU hit |
| `miner/miner-base/src/miner_base/block_submission.py` | Selected strips and Merkle proof packaging |
| `miner/miner-base/src/miner_base/settings.py` | Current noise, tile, and `2 x 64` hash pattern settings |
| `miner/miner-base/src/miner_base/noisy_gemm.py` | Current Python reference for int8 inputs, noise, int32 products, and correction |
| `miner/pearl-gemm/csrc/gemm/kernel_traits.hpp` | CUDA main GEMM input element `int8_t`, accumulator `int32_t` |
| `miner/pearl-gemm/csrc/gemm/collective_mainloop.hpp` | CUDA tiled GEMM and per-rank-chunk hash signal path |
| `miner/pearl-gemm/csrc/gemm/pow_utils.hpp` | XOR reduction, rotate-left 13, keyed BLAKE3, and little-endian U256 comparison |
| `miner/vllm-miner/src/vllm_miner/quantization_operators.py` | Symmetric 7-bit/8-bit quantization, fp32 scale, no zero point |
| `miner/pearl-gemm/src/pearl_gemm/quantization/hadamard.py` | `max_val=63`, round-to-nearest, clamp, optional Hadamard preprocessing |
| `zk-pow/src/api/proof.rs` | Header/config/proof schema and `Int7xInt7ToInt32` enum |
| `zk-pow/src/api/sanity_checks.rs` | Current rank, dimension, tile, worker-size, and target checks |
| `zk-pow/src/circuit/pearl_noise.rs` | Exact noise labels, byte mapping, sparse pair, and low-rank factor construction |
| `zk-pow/src/ffi/mine.rs` | CPU mining/reference path, commitments, noised products, transcript, and `PlainProof` |
| `zk-pow/src/circuit/pearl_program.rs` | `TILE_D=16`, `TILE_H=2`, 16-word jackpot, rotate-left 13, circuit layout |
| `node/rpcserver.go` and `node/btcjson/chainsvrresults.go` | `getblocktemplate`, `submitblock`, `requiredcertversion`, and RPC version |
| `node/chaincfg/params.go` | Mainnet fork heights and required certificate-version selection |
| `node/docs/mining.md` | Direct-node mining and Taproot `miningaddr` requirements |

At the pinned mainnet source state, the node's relevant fork-height values are
`MoEForkHeight=71935`, `DenseOnlyForkHeight=91630`, and
`RankPenaltyForkHeight=96251`. These are source-pinned facts for certificate
and target rules, not a reason to hard-code a future network configuration.

The root README identifies the current official miner as a CUDA/vLLM path for
Hopper `sm90` hardware (H100/H200). No Pearl GPU benchmark was imported into
this project.

## Protocol findings

The direct node uses HTTP(S) JSON-RPC with BIP22-style methods. The gateway
requests:

```json
{"capabilities":["coinbasevalue","workid","coinbase/append"],"rules":["segwit"]}
```

through `getblocktemplate`, then submits a complete block hex string through
`submitblock`. The gateway's local miner interface is not Stratum: it is
newline-delimited JSON-RPC over `/tmp/pearlgw.sock` by default or loopback TCP
8337 when configured. The two observed methods are:

- `getMiningInfo` with `{}` → base64 incomplete header, target, and certificate
  version;
- `submitPlainProof` with `plain_proof` base64 and a `mining_job` object →
  asynchronous gateway submission.

No `stratum`, `mining.subscribe`, `mining.authorize`, `mining.notify`, or
`mining.submit` implementation was found in the pinned Pearl tree. The proof
library's `nbits_override` allows a share target to be validated, but it does
not define an external pool transport or payout protocol. Pool support is
therefore `UNKNOWN` and remains out of P0.

## P1 clean-room and black-box record

P1 inspected the pinned source only to write an independent behavior record;
the following Pearl hot-component files were not copied, translated, or
structurally reproduced in `src/pearl/`:

- `miner/pearl-gemm`, `miner/pearl-gateway`, `miner/miner-base`,
  `py-pearl-mining`, `zk-pow`, and `pearl-blake3`.

The P1 implementation's only cryptographic dependency is the official BLAKE3
crate `blake3 = 1.8.2`, pinned in
`src/pearl/blake3_ffi/Cargo.toml` and `Cargo.lock`. Cargo metadata reports
`CC0-1.0 OR Apache-2.0 OR Apache-2.0 WITH LLVM-exception`; P1 records the
compatible `CC0-1.0 OR Apache-2.0` license expression in its manifest. The
helper uses public one-shot keyed hashing and the public `hazmat` chunk/parent
CV APIs through a minimal C ABI. It does not use Pearl's local BLAKE3 code.

P1 source facts and black-box checks:

| Behavior | Pinned source/tool | Input/output record | Result |
|---|---|---|---|
| BLAKE3 helper/Merkle implementation tests | external `/tmp/pearl-p1-audit/pearl-blake3`, commit `fe22b6a2...` | `cargo test --manifest-path .../pearl-blake3/Cargo.toml`; 35 tests | 35 passed |
| Job key | external comparator using the pinned `pearl-blake3` crate | 108-byte header/config preimage; `13038bff01365936baf6f890b92cbdc3fc1bc4d5f9ae9cd13dc33ce1bdbb6fb5` | match |
| 1024-byte Merkle tree | external comparator `cargo run --release` in `/tmp/pearl-p1-blackbox` | 2048 bytes `00..ff` repeated, key `11`×32; root `aa17a0831b07bb7ed899783326e09ee7f4cfde523218c14c7eaedeeb069f7531` | match |
| Full Pearl miner | no pinned standalone binary/tool available | no observable output | no claim |

The fixed corpus and all P1 output values are in
`tests/data/pearl/p1/vectors.json`. The P1 evidence record includes the
corpus SHA-256, dependency details, test counts, and unresolved limitations.

## License review and code-reuse decision

The root `LICENSE` identifies an ISC root and component-specific licenses:

| Component | Observed license | P0 reuse decision |
|---|---|---|
| Root / `node` / `wallet` | ISC | Facts may be re-expressed; no source copy needed |
| `spv` | MIT | Not needed for P0; review any future reuse separately |
| `dnsseeder` | Apache-2.0 | Not needed for P0; review any future reuse separately |
| `plonky2` | MIT OR Apache-2.0 | Dependency candidate only; no code copied |
| `xmss/external` | CC0-1.0 | Not needed for P0 |
| `miner/pearl-gemm/third_party/cutlass` | BSD-3-Clause at pinned gitlink | Not copied; any future use needs notices and build review |
| `miner/pearl-gemm`, `miner/pearl-gateway`, `miner/miner-base`, `py-pearl-mining`, `zk-pow`, `pearl-blake3` | No clear component-level license grant or SPDX declaration was found in the inspected tree; `vllm-miner` package metadata says MIT but does not grant the other components | **Do not copy code; clean-room implementation only until legal review resolves the boundary** |

This is not a claim that Pearl has no license. It is a finding that the exact
hot miner, gateway, proof, and binding components do not have a clear local
reuse grant in the pinned tree. P0 therefore permits independent
documentation of protocol facts, formats, mathematics, and test vectors, but
not source-code copying or source-structure translation. A full dependency
SBOM and notice audit is a later gate before any distribution or adapter reuse.

## Reproducibility commands

Run these outside the project working tree so the upstream checkout cannot
silently become a project dependency:

```bash
git clone https://github.com/pearl-research-labs/pearl.git /tmp/pearl-audit
git -C /tmp/pearl-audit checkout fe22b6a2b831d95b2f56564808f39d2f498f34a5
git -C /tmp/pearl-audit rev-parse HEAD
sha256sum /tmp/pearl-audit/Pearl_Whitepaper.pdf
git -C /tmp/pearl-audit ls-tree HEAD miner/pearl-gemm/third_party/cutlass
```

The audit used no live Pearl node, pool, wallet, or submission endpoint.

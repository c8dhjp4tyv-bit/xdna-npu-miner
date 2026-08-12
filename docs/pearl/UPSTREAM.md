# Pearl (PRL) P0 Upstream, Protocol, and License Record

## Current Certificate V3 compatibility update (2026-08-12)

This section is a new record and does not revise the historical Certificate V2
evidence below. At shot start, a fresh official fetch resolved `master` to
`bfd064717de4af0e8471bdc24ca4a28aa6278227`, exactly the requested current
revision. It is Pearl `1.4.2`; it is four commits ahead of the historical P7
pin `fe22b6a2b831d95b2f56564808f39d2f498f34a5`.

The compatibility review is machine-readable in
`docs/evidence/pearl-v3-upstream-audit.json`. Every consensus-relevant field
was resolved before implementation: no field remains `UNKNOWN`. The current
official runtime observed for the new SIMNET proof was `pearld 1.4.2`
(`SHA-256 be2b06f5d2782b737785cbab115480c206092a3db3f7bb2a1f9b4b5bf4a4cbbf`),
`pearl-gateway 0.1.0`, and `py-pearl-mining 0.3.0` in a CPU-only Python 3.12.13
environment. The official CUDA/vLLM miner was neither installed nor started.

### V3 consensus facts

Commit `fc5ca65a1df0fad0140e74c3b52e71c4a0f99e90` introduces Certificate V3
salted noise seeds. Commit `fadd42af05ad6b6a5b69ee29913fcf2e60eea4c0` sets
the current mainnet `SaltedSeedForkHeight` to 99000. That height is protocol
context only: the miner never chooses a certificate version from height. It
requires `cert_version` from gateway `getMiningInfo` (and treats the node
template's `requiredcertversion` as authoritative where available), stores it
in immutable job identity, and supports only 1, 2, and 3.

For V3, raw keyed Merkle roots remain on the proof/share wire. The CPU binds
them only for seed derivation:

```text
key_A = BLAKE3("pearl/cert-v3/noise-seed/A")
key_B = BLAKE3("pearl/cert-v3/noise-seed/B")
bound_A = keyed_BLAKE3(key_A, raw_hash_A || m_le32 || 28 zero bytes)
bound_B = keyed_BLAKE3(key_B, raw_hash_B || n_le32 || 28 zero bytes)
b_noise_seed = BLAKE3(job_key || bound_B)
a_noise_seed = BLAKE3(b_noise_seed || bound_A)
```

Each binding input is exactly 64 bytes. The independently derived domain-key
digests and canonical V3 vector corpus are recorded in
`docs/evidence/pearl-v3-cpu-vectors.json`; the reference test rejects wrong
endianness, dimension/salt swaps, 27/29-byte padding, and both accidental
V2-salted and V3-unsalted paths. The current proof commitment domain is
`SHA256d(cert_version_le32 || public_data)`, so V3 uses a `3` prefix rather
than inheriting V2's prefix.

The current `PlainProof` Rust/bincode layout, base64 field, and gateway
submission schema remain compatible with the V2 layout. `cert_version` is a
mandatory `MiningJob` field. Certificate V3 retains raw Merkle roots on that
wire; substituting bound roots is invalid. The project’s historical local P1
envelope is deliberately not sent for V3: an audited official-wire serializer
is required.

`bfd0647` also raises the minimum peer protocol version to 2. Read-only
mainnet preparation must therefore verify connected peers speak version 2 and
must never substitute a 1.3.x node.

## One-shot implementation audit update (2026-08-11)

The pinned official repository still resolves to master
`fe22b6a2b831d95b2f56564808f39d2f498f34a5`; no Pearl source was copied into
this repository. The project-owned IRON/AIE2 build used MLIR-AIE commit
`57d7494e99c214f5f53b328a0ed43a99e759e835`. The physical host observed
`RyzenAI-npu1`, AIE2, XRT 2.26.0, firmware 1.5.5.391, and amdxdna rc7. The
historical runtime pin remains unchanged and the difference is recorded in
P2/P9 evidence.

P4/P6 clean-room transport matches the pinned local methods and framing. P7
then built the pinned official `pearld` (`1.3.1`, SHA-256
`b894adba2bfb1c02dcb99599fc4ab9e796e88cc44e88865751639d70a92d0f75`) and ran
the official gateway/prover in an isolated CPU-only Torch environment. The
physical XDNA1 path submitted an official-wire PlainProof and the local
SIMNET node accepted the resulting block; exact evidence is in
`docs/evidence/pearl-p7-e2e.json`. The official CUDA/vLLM miner was not
installed or launched. No official Stratum/pool protocol was found; record
`POOL_PROTOCOL_UNAVAILABLE` rather than adding inferred compatibility.

## Source pins

Audit date: **2026-08-11**. Only the official Pearl repository and official
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
| `miner/py-pearl-mining/` | Official Python proof API used by the gateway/prover; installed externally as `py-pearl-mining 0.2.0` |
| `miner/pearl-gateway/pyproject.toml` | Official gateway package metadata; installed externally as `pearl-gateway 0.1.0` |
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
therefore `POOL_PROTOCOL_UNAVAILABLE` and remains outside the P7 SIMNET pass.

### P7 runtime and wire findings

The official local gateway exposes `getMiningInfo` and `submitPlainProof` over
newline-delimited JSON-RPC. Its `PlainProof` payload is a Rust/bincode object:
the four dimensions are followed by the two `MatrixMerkleProof` values and a
dense-proof `Option::None` tag. The project adapter serializes this official
wire separately from its P1 evidence envelope; the accepted 140225-byte
payload is proof of interoperability. Consensus acceptance did not require a
model-identity signature: commitments, openings, deterministic noise,
transcript, and target checks were sufficient for the dense SIMNET fixture.

The official raw signal validation is inclusive `[-64,64]`. Noised operands
can exceed that raw-source interval while remaining valid signed-int8 physical
inputs, so the project boundary validates raw matrices before noising and does
not reject the noised values against the raw bound.

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

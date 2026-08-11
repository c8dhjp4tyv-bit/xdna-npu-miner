# Upstream Sources and Licensing

Audit date: **2026-08-10**.

This registry is the source of truth for M0's current Qubic claims. Source code
was inspected in separate checkouts under /home/umutcagand/qubic-m0.WHEa7H; none
of that source was copied into this repository.

## Pearl source registry

Pearl is a separate active research target. Its official revision, mining and
node path, whitepaper hash, license boundary, and clean-room reuse policy are
recorded in [`docs/pearl/UPSTREAM.md`](pearl/UPSTREAM.md). Do not mix Pearl
facts or source with the Qubic records below. Pearl P0 did not copy upstream
source and did not make a live connection. Pearl P1 adds only an independent
CPU oracle and a pinned third-party BLAKE3 dependency; it does not reuse Pearl
hot-component source.

### Pearl P7 runtime record

P7 built the pinned official runtime only in an external `/tmp` checkout and
proved the official local SIMNET acceptance path. The exact runtime and block
evidence is in [`docs/pearl/UPSTREAM.md`](pearl/UPSTREAM.md) and
[`docs/evidence/pearl-p7-e2e.json`](evidence/pearl-p7-e2e.json). No official
CUDA/vLLM miner was installed or launched.

### Pearl P1 implementation dependency record

- `src/pearl/blake3_ffi/` pins the official `blake3` Rust crate at `1.8.2`.
  The crate metadata declares `CC0-1.0 OR Apache-2.0 OR Apache-2.0 WITH
  LLVM-exception`; the P1 manifest records the compatible `CC0-1.0 OR
  Apache-2.0` expression.
- P1 uses the crate's public keyed hash and hazmat chunk/parent-CV APIs only
  through a minimal C ABI. No code was copied from Pearl's `pearl-blake3`.
- The pinned Pearl tree is
  `fe22b6a2b831d95b2f56564808f39d2f498f34a5`. An external black-box checkout
  ran `cargo test --manifest-path /tmp/pearl-p1-audit/pearl-blake3/Cargo.toml`
  with 35 passing tests, and a separate comparator matched the P1 job key
  `13038bff01365936baf6f890b92cbdc3fc1bc4d5f9ae9cd13dc33ce1bdbb6fb5` and
  Merkle root
  `aa17a0831b07bb7ed899783326e09ee7f4cfde523218c14c7eaedeeb069f7531`.
- The comparator used the source checkout only as a black box and wrote no
  Pearl implementation into this repository. Full-miner binary comparisons
  were unavailable and are not claimed.

## Evidence hierarchy

1. Current Qubic core/node source and its pinned production task define the
   algorithm, task validation, and direct-node verification behavior.
2. Current Qiner is a useful official/current reference miner for candidate
   construction and direct solution packaging, but its example task and
   compile-time threshold do not match the core production task.
3. Official Qubic documentation defines high-level mining/pool status. It does
   not replace source for byte-level behavior.
4. The QLI client documents one independently operated pool client. It is not a
   canonical Qubic protocol specification.

The core protocol document explicitly calls itself incomplete/non-final and
says implementation details may differ. Where it conflicts with source, the
current source path is the evidence used here.

For this project, the pinned Qubic core revision
`a83f935406cd006b5b1a94971139e74d410ecb6d` is the canonical source of
consensus constants and validation truth. Qiner at
`11fb18a6f4944bb55fe103d3f263cb5d31e00200` is a behavioral/reference aid for
candidate construction and direct-node submission. Qiner's example task and
example threshold are not production consensus values. Future agents must
re-check the current upstream revisions before implementing network-facing
behavior because Qubic mining is actively changing.

M6 revalidated both public Git remotes on 2026-08-10 with `git ls-remote`.
Qubic core `HEAD`, `main`, and `v1.301.3` all resolved to
`a83f935406cd006b5b1a94971139e74d410ecb6d`; Qiner `HEAD`, `main`, and
`v1.302.3` all resolved to `11fb18a6f4944bb55fe103d3f263cb5d31e00200`.
The direct-node implementation uses only independently expressed framing,
parsing, freshness, gating, and serialization; no upstream source was copied.

## Source records

### Source S-001 — Qubic core/node

- **Project:** qubic/core
- **Authority:** Official/current protocol and node implementation
- **Repository:** https://github.com/qubic/core
- **Revision inspected:** tag v1.301.3, commit
  a83f935406cd006b5b1a94971139e74d410ecb6d; main pointed to this commit
  during the audit (2026-08-09).
- **Important paths:**
  - src/mining/score_common.h
  - src/mining/score_bpp9000.h
  - src/test/score_bpp9000_reference.h
  - src/mining/task_file.h
  - data/bpp9000.task
  - src/public_settings.h
  - src/score.h
  - src/qubic.cpp
  - src/network_messages/header.h
  - src/network_messages/network_message_type.h
  - src/network_messages/system_info.h
  - src/network_messages/broadcast_message.h
  - src/mining/mining.h
  - doc/protocol.md
  - LICENSE.md, LICENSE-MIT.md
- **Revision links:** https://github.com/qubic/core/commit/a83f935406cd006b5b1a94971139e74d410ecb6d,
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/mining/score_bpp9000.h,
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/mining/task_file.h,
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/network_messages/network_message_type.h.
- **License:** Custom Anti-Military License in LICENSE.md; no SPDX identifier.
  LICENSE-MIT.md is a narrow exception for src/platform/uint128.h and
  test/uint128.cpp and is not a license for the scorer or node.
- **Reuse decision:** Reference only / clean-room required for this project.
  Do not copy or adapt core scorer, node, crypto, or protocol source.
  Independently reimplement observable behavior and cite this record. If code
  reuse is proposed later, perform a file-level legal review first.
- **Attribution/constraints:** A permitted copy of the custom-licensed portion
  would require its notice and would carry Anti-Military field-of-use and
  military-connection restrictions. The target project's license is not yet
  finalized, so compatibility cannot be assumed.

### Source S-002 — Qiner reference miner

- **Project:** qubic/Qiner
- **Authority:** Current Qubic reference miner
- **Repository:** https://github.com/qubic/Qiner
- **Revision inspected:** tag v1.302.3, commit
  11fb18a6f4944bb55fe103d3f263cb5d31e00200; main pointed to this commit
  during the audit.
- **Important paths:** src/score_bpp9000.h, src/task_file.h, src/Qiner.cpp,
  README.md, data/example_task_bpp9000.bin, LICENCE, NOTICE.md.
- **Revision links:** https://github.com/qubic/Qiner/commit/11fb18a6f4944bb55fe103d3f263cb5d31e00200,
  https://github.com/qubic/Qiner/blob/11fb18a6f4944bb55fe103d3f263cb5d31e00200/src/score_bpp9000.h,
  https://github.com/qubic/Qiner/blob/11fb18a6f4944bb55fe103d3f263cb5d31e00200/src/Qiner.cpp.
- **Behavior established:** BPP9000 candidate initialization, random2 usage,
  nonce encoding, mutation search, scalar/reference score semantics, and
  direct-node solution broadcast construction.
- **Important discrepancy:** Qiner's example task is 44,744 bytes but has
  SHA-256 403e24225f5b0512d0cbf49758fed9a01e7334d3cea565ad6c5e82420b713226,
  different task hashes, and an example threshold of 6,469 from
  (numberWindows - 1) * 4 / 5. It is not the core production task.
- **License:** Custom Anti-Military License in LICENCE; no SPDX identifier.
  NOTICE.md identifies Catch2 as Boost Software License 1.0 (BSL-1.0).
- **Reuse decision:** Reference only / clean-room required. Do not copy Qiner
  source. Protocol facts, file formats, and independently expressed behavior
  may guide a separate implementation. A permitted Qiner copy would require
  the license notice and its use restrictions; Catch2 has separate BSL-1.0
  terms.

### Source S-003 — Official Qubic documentation

- **Authority:** Official public documentation and FAQ
- **Sources inspected:**
  - https://qubic.org/mining
  - https://docs.qubic.org/learn/sw/
  - https://docs.qubic.org/learn/pool/
  - https://qubic.org/faq
  - https://qubic.org/blog-detail/qatum-protocol-a-stratum-like-mining-protocol-for-qubic
  - https://qubic.org/ecosystem/qatum-protocol
- **Accessed:** 2026-08-09.
- **Behavior established:** High-level mining material describes epochs and
  useful proof of work; pool documentation says pools are independently
  operated and their terms/protocols vary. The FAQ describes Qatum as a
  stratum-like Qubic mining protocol currently in development.
- **Reuse decision:** Documentation/protocol facts may be independently
  implemented. No source code is being reused.
- **Open consequence:** There is no sufficiently complete, stable authoritative
  Qatum message schema. A pool adapter cannot be specified as interoperable
  until a versioned protocol or implementation is published and independently
  reviewed; this is a deferred optional adapter gate, not an M0 blocker.

### Source S-004 — QLI pool client

- **Project:** qubic-li/client (qli-client)
- **Authority:** Maintainer-operated pool client documentation; not Qubic core
- **Repository:** https://github.com/qubic-li/client
- **Revision inspected:** tag v3.7.2, commit
  9a01902342240c69b19d9cceb637ea68916a3d2c; main pointed to this commit
  during the audit.
- **Important paths:** README.md, CustomRunner.md.
- **Revision link:** https://github.com/qubic-li/client/commit/9a01902342240c69b19d9cceb637ea68916a3d2c.
- **Behavior established at documentation level:** The QLI client uses a
  WebSocket pool endpoint documented as wss://wps.qubic.li/ws, a JWT
  AccessToken, a payout Qubic address/alias, PPS or solo settings, and a
  trainer/worker that the client may download/update. CustomRunner.md
  describes an older stdout runner integration; it is not a canonical current
  worker wire protocol.
- **Transport/auth/job/result limits:** The checked-in documentation does not
  provide a complete stable frame schema, job message schema, share submission
  schema, or reconnect contract suitable for clean-room interoperability. The
  QLI client is an independently operated service/client.
- **License:** No license file or SPDX declaration was found in the inspected
  repository.
- **Reuse decision:** Unresolved / reference only. Do not copy source,
  protocol assumptions, or client behavior into this project. An eventual
  adapter requires an explicit license review and a pinned service protocol.

### Qatum status conflict and M0 scope decision

The official Qubic sources currently disagree about Qatum's status:

- The current FAQ, accessed 2026-08-09, describes Qatum as “a stratum-like
  mining protocol for Qubic, currently in development”:
  https://qubic.org/faq
- An official Qubic Team blog post published 2025-02-28 says that Qatum “has
  now launched” and is officially live at Qatum.org:
  https://qubic.org/blog-detail/qatum-protocol-a-stratum-like-mining-protocol-for-qubic
- The current official ecosystem entry labels Qatum **Live**:
  https://qubic.org/ecosystem/qatum-protocol

These status statements do not provide a sufficiently complete, authoritative,
versioned wire specification for job distribution, subscription/version
negotiation, shares, authentication, reconnect, or task ownership. This
project therefore does not choose a definitive current Qatum wire behavior.

The direct Qubic-node path is the canonical protocol path, and the direct-node
behavior verified from the pinned core/Qiner sources is sufficient for M0.
M1 through M5 have zero dependency on Qatum or any pool. M6 must implement and
validate direct-node integration first. Qatum/pool integration is optional
after that path works and must wait until an authoritative, sufficiently
complete specification or implementation can be pinned and independently
reviewed. Pool-specific proprietary protocols are adapters, not part of the
mining/scoring core. Do not copy Anti-Military-licensed upstream source into
this repository.

### Source S-005 — Hawk Point XDNA1 reference environment

- **Project:** c8dhjp4tyv-bit/hawkpoint-npu-llm
- **Repository:** https://github.com/c8dhjp4tyv-bit/hawkpoint-npu-llm
- **Revision inspected:** branch revert/kernel-pin-441, commit
  15f3352779e944ccd202b2e166e40e197a1de759.
- **Important paths:** release-pins.json, scripts/configure-hardware-runner.sh,
  SUPPORT.md, project LICENSE/NOTICE.
- **Behavior established:** A validated Fedora/Hawk Point setup uses
  RyzenAI-npu1, XDNA1/AIE2, four columns, XRT, MLIR-AIE/IRON, explicit buffer
  sync, persistent device state, and hardware/telemetry checks.
  release-pins.json recorded AMD XDNA package
  7.2.0-0.rc5.260729.fc02acf6.441.vanilla.fc45.x86_64, firmware 1.5.5.391,
  XRT 2.26.0, and XRT hash
  8bf2fc4c090540dcf7872243ab67779ae74ef5e3.
- **License:** Project Apache-2.0/LLVM-exception notices; optional Ollama
  components are MIT. This is a separate repository.
- **Reuse decision:** Reference only. No source or build coupling is imported.
  M2 must validate and pin this project's own dependency set.

### Source S-006 — MLIR-AIE/IRON reference

- **Project:** Xilinx/mlir-aie
- **Repository:** https://github.com/Xilinx/mlir-aie
- **Revision inspected:** commit
  57d7494e99c214f5f53b328a0ed43a99e759e835 as selected by the related
  Hawk Point environment.
- **License:** Apache-2.0 with LLVM exception files as applicable to the
  repository; verify exact third-party notices when importing a dependency.
- **Reuse decision:** Selected external runtime/toolchain dependency for the
  M2 smoke, pinned in `runtime-pins.json`; no MLIR-AIE source is copied into
  this repository. Exact third-party notices remain a release/legal review
  item before redistribution.

### Source S-007 — M2 current host/runtime validation

- **Purpose:** Project-owned record of the physical runtime used for the M2
  acceptance smoke; this is not a portable hardware-support guarantee.
- **Host:** Fedora Linux 45 prerelease, kernel/amdxdna
  `7.2.0-0.rc5.260731.8ba098e6.443.vanilla.fc45.x86_64`, AMD Ryzen 7 250 with
  Radeon 780M.
- **Device:** XRT-opened `RyzenAI-npu1`, AIE2, BDF `0000:06:00.1`,
  `/dev/accel/accel0`; `xrt-smi examine` reports topology `6x5` and four
  available columns.
- **Runtime pins:** firmware `1.5.5.391`; XRT `2.26.0`, hash
  `8bf2fc4c090540dcf7872243ab67779ae74ef5e3`; MLIR-AIE commit
  `57d7494e99c214f5f53b328a0ed43a99e759e835`; `mlir_aie` `1.3.4`; CPython
  `3.12.13`; `llvm-aie 21.0.0.2026072001+ce8c0f8f`.
- **Project artifacts:** `src/xdna/smoke_program.py` compiled one AIE2 column
  into an `MLIR_AIE` XCLBIN and instruction stream. The artifact UUID and
  hashes are recorded in `runtime-pins.json` and
  `docs/evidence/m2-xdna-smoke.json`.
- **Validation commands:** `./scripts/verify-xdna1.sh`,
  `./scripts/build-xdna-smoke.sh`, and
  `./scripts/run-xdna-smoke.sh --iterations 100`.
- **Observed result:** 100 physical XRT dispatches, 100 completions, 100 exact
  CPU-oracle matches, zero mismatches, zero runtime failures, 200 H2D and 100
  D2H synchronizations. This is correctness/dispatch evidence, not a
  performance measurement.
- **Source/reuse decision:** XRT and MLIR-AIE are external runtime/toolchain
  dependencies selected from the installed environment. No source was copied
  from `hawkpoint-npu-llm`; the related repository remains reference-only.

### Source S-008 — Independently sourced K12/FourQ production provider

- **Purpose:** M6's optional production crypto adapter only. This selection
  does not copy Qubic core/Qiner crypto and does not enable live submission by
  itself.
- **FourQ project:** Microsoft FourQlib v3.1,
  https://github.com/microsoft/FourQlib
- **FourQ revision:** `1031567f23278e1135b35cc04e5d74c2ac88c029` (the
  repository's current `master` at the 2026-08-09 audit; no release tag was
  available in the checked remote).
- **FourQ license:** MIT, from the repository LICENSE. The selected API and
  implementation files are `FourQ_64bit_and_portable/FourQ_api.h`,
  `eccp2.c`, `eccp2_no_endo.c`, `eccp2_core.c`, `crypto_util.c`,
  `schnorrq.c`, `kex.c`, and `random/random.c`.
- **FourQ functions used:** `CompressedPublicKeyGeneration`,
  `CompressedSecretAgreement`, `SchnorrQ_Sign`, and `SchnorrQ_Verify`.
  FourQlib's documented `crypto_sha512` hook is supplied by this repository
  and is backed by KT128; the bundled SHA-512 implementation is not compiled.
- **K12 project:** XKCP extracted KangarooTwelve implementation,
  https://github.com/XKCP/K12
- **K12 revision:** `f95b0b73e29fe75fe99fbbb24c8000d9fcf0b40e` (remote
  `master` at the 2026-08-09 audit).
- **K12 selected files:** `lib/KangarooTwelve.c`,
  `lib/KangarooTwelve-threading.c`,
  `lib/Optimized64/KeccakP-1600-opt64.c`, plus their K12/Plain64 headers.
  These selected implementation files carry the implementer's CC0/public-
  domain dedication. The optimized permutation includes
  `lib/brg_endian.h`, which carries Brian Gladman's permissive redistribution
  notice; that notice remains part of the dependency audit.
- **Adapter files:** `src/qubic/production_crypto.hpp/.cpp`, enabled only by
  `-DXDNA_ENABLE_PRODUCTION_CRYPTO=ON`. `SigningSecret` is treated as the
  32-byte Qubic signing subseed; the adapter derives the FourQ scalar as
  `KT128(subseed, 32 bytes)` and checks the configured public key against that
  derivation before signing or ECDH.
- **KAT sources and coverage:** RFC 9861 KangarooTwelve vectors for empty
  input and one zero byte,
  https://www.rfc-editor.org/rfc/rfc9861.html; the first synthetic Qubic
  SchnorrQ vector from
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/test/fourq.cpp;
  and fixed project vectors for FourQ public keys, the shared key, the
  K12-derived gamming key, and the 68-byte gamma stream in
  `tests/qubic_crypto_tests.cpp`. The fixed key inputs are synthetic test
  material, not operator secrets.
- **Reuse decision:** Approved as an optional external dependency after the
  file/function/license review above. No Qubic/Qiner crypto source was copied;
  the provider remains a separately injected component. The read-only live
  system-info probe is recorded in Source S-009; solution submission remains
  unexercised.

### Source S-009 — Official direct-network endpoint and live read-only revalidation

- **Authority:** Official Qubic Team public direct-network guidance and the
  current official core node implementation.
- **Endpoint source:** The Qubic Team's direct-network article documents the
  Direct Network endpoint `corenet.qubic.li:21841`:
  https://qubic.org/blog-detail/how-to-query-qubic-oracle-machines-using-the-qubic.net-toolkit
- **Peer source:** The current core README directs operators to
  `https://app.qubic.li/network/live` for known public peers and describes the
  listen-only peer configuration:
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/README.md
- **Accessed/revalidated:** 2026-08-10 (with the initial live pass on
  2026-08-09). `corenet.qubic.li` resolved to public
  IPv4 addresses and TCP `21841` accepted a bounded connection. The project
  probe then sent the clean-room peer-exchange handshake plus
  `REQUEST_SYSTEM_INFO` and received two valid `RESPOND_SYSTEM_INFO` frames;
  sanitized values are recorded in `docs/evidence/m6-direct-node.json`.
- **Current wire paths:**
  `src/network_messages/header.h`,
  `src/network_messages/network_message_type.h`,
  `src/network_messages/system_info.h`,
  `src/network_messages/broadcast_message.h`, and `src/qubic.cpp` at core
  `a83f935406cd006b5b1a94971139e74d410ecb6d`. A new connection sends an
  `EXCHANGE_PUBLIC_PEERS` frame (type 0, 16-byte payload); a direct request is
  type 46 and its response is type 47 with a packed 128-byte payload. A node
  may stream ordinary network frames on the same connection, so the adapter
  filters those before accepting the system-info response.
- **Submission constraint:** Core's protocol documentation and current source
  require a nonzero source public key, source authorization through the
  computor or dissemination-balance rule, and a destination matching a current
  computor public key. System info does not provide the destination key. An
  ephemeral or guessed identity is therefore not a valid live submission
  substitute. No submission frame was sent.
- **Reuse decision:** Endpoint and observable wire facts are independently
  implemented and recorded; no Qubic core/Qiner source was copied. The public
  probe is read-only and does not establish task-byte compatibility or
  submission acceptance.

### Source S-010 — Current submission authorization and public work-data acquisition

- **Audit date:** 2026-08-10. The official core and Qiner `main` refs were
  rechecked with `git ls-remote`; they remain core
  `a83f935406cd006b5b1a94971139e74d410ecb6d` (`v1.301.3`) and Qiner
  `11fb18a6f4944bb55fe103d3f263cb5d31e00200` (`v1.302.3`).
- **Exact core authorization paths:**
  `src/qubic.cpp::processBroadcastMessage` requires a nonzero source public
  key and a valid signature over the BroadcastMessage body. It sets
  `hasEnoughBalance` only when `spectrumIndex(source) >= 0` and
  `energy(spectrumIndex(source)) >= MESSAGE_DISSEMINATION_THRESHOLD`, where
  `MESSAGE_DISSEMINATION_THRESHOLD` is exactly `1000000000`. Relay is allowed
  for a funded source, a current computor, or the dispatcher. A non-computor
  source targeting a current computor is processed only when the same funded
  balance condition holds; a computor source uses the encrypted shared-key
  path. The destination must match one of the current `computorPublicKeys`.
  The same function decrypts the 68-byte solution payload and accepts it only
  when the claimed score equals the node's recomputed score, the score is
  valid, and it is at or below the runtime threshold.
- **Current computor acquisition paths:**
  `src/network_messages/network_message_type.h` defines
  `REQUEST_COMPUTORS=11` and `BROADCAST_COMPUTORS=2`; `src/network_messages/computors.h`
  defines the packed response as epoch, 676 32-byte public keys, and a
  64-byte signature. `src/qubic.cpp::processRequestComputors` returns the
  node's current list. The bounded read-only helper
  `scripts/run-m6-live-computors.sh` queried this path on
  `corenet.qubic.li:21841` at `2026-08-10T06:13:37Z`: response type 2,
  payload 21698 bytes, epoch 225, 676 nonzero keys, key-list SHA-256
  `58ef30a7fece845226c91502ff616747e1d50aab34ef530e68e15a36231aa9bf`, and
  signature SHA-256
  `be0db535e84b1ac1e78f689b86c882f17031558b6729cce727a8f937011e0ff6`.
  The list epoch matched the live system-info epoch 225. The helper does not
  claim to verify the Arbitrator signature; it only validates framing, exact
  size, epoch extraction, and nonzero keys.
- **Current production task source:**
  core `src/public_settings.h` pins the BPP9000 task filename
  `bpp9000.task`, its topology/data K12 hashes, production dimensions, and
  threshold. `src/qubic.cpp::loadBpp9000Task` loads `data/bpp9000.task`, checks
  its exact size/header, recomputes both block hashes with K12, and refuses to
  score if the hashes or topology are wrong. At the audited core revision the
  official file is 44744 bytes with SHA-256
  `0c5e9e42c6d86c320af62f4125ca85b2446f2b098893fd6521bcf66c22f7f00a`; its
  header carries topology hash
  `13e99d5b2fca56aa789cb959575f48392f1a44909a8eaf27f2de8f8d74b07a6b` and
  data hash
  `979cdc2247d2ca4ed3d614bf27896384cb1c9c3d804af6ede6b59fc52c0e3dfa`.
  The task is an authoritative pinned core input, not a task payload returned
  by SystemInfo; this project does not copy it into the repository or claim
  that the live node advertised task bytes.
- **Qiner submission reference:** current `src/Qiner.cpp` accepts a mining
  identity/destination, a separate signing seed, the mining seed, and an
  optional task-file path. It constructs a type-1 BroadcastMessage, derives
  the solution message gamma, encrypts mining seed/nonce/score, signs the body,
  and sends it. Qiner does not provision a source identity or fund its
  dissemination balance; those are operator-owned prerequisites.
- **Legitimate identity path:** official Qubic CLI documentation exposes
  `-showkeys`, `-getbalance`, and `-sendtoaddress`; official wallet guidance
  describes creating a 55-character seed and deriving its Qubic ID. A newly
  generated identity is not authorized by core until it exists as a spectrum
  entity with at least 1000000000 energy, and a computor identity requires its
  corresponding current computor secret. No user-owned seed, computor secret,
  or funded source was present in the runtime environment, and no funds were
  transferred.
- **Source links:**
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/qubic.cpp,
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/network_messages/computors.h,
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/network_messages/network_message_type.h,
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/public_settings.h,
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/data/bpp9000.task,
  https://github.com/qubic/Qiner/blob/11fb18a6f4944bb55fe103d3f263cb5d31e00200/src/Qiner.cpp,
  https://docs.qubic.org/developers/qubic-cli/, and
  https://docs.qubic.org/learn/invest/.
- **Reuse decision:** These paths are source and protocol references only.
  The task bytes, core/Qiner source, and any identity material remain outside
  this repository. The M6 live gate remains blocked by the external authorized
  source identity and secret, not by a guessed destination or synthetic task.

### Source S-011 — Read-only authorization, identity encoding, and pinned task cache

- **Audit date:** 2026-08-10. The implementation uses only the pinned core
  `a83f935406cd006b5b1a94971139e74d410ecb6d` as protocol authority.
- **Network message enum:**
  https://raw.githubusercontent.com/qubic/core/a83f935406cd006b5b1a94971139e74d410ecb6d/src/network_messages/network_message_type.h
  defines `REQUEST_COMPUTORS=11`, `BROADCAST_COMPUTORS=2`,
  `REQUEST_ENTITY=31`, `RESPOND_ENTITY=32`, `END_RESPONSE=35`, and the
  system-info types 46/47. The local reader filters ordinary peer/request
  traffic but never treats `END_RESPONSE` as success.
- **Entity wire schema:**
  https://raw.githubusercontent.com/qubic/core/a83f935406cd006b5b1a94971139e74d410ecb6d/src/network_messages/entity.h
  defines `EntityRecord` as a 64-byte packed record and `RespondEntity` as
  840 bytes after its 24 sibling public keys. `energy(index)` is
  `incomingAmount - outgoingAmount` in the pinned spectrum implementation.
- **Computor authorization:** the packed list is 2-byte epoch + 676 public
  keys + 64-byte signature (21,698 bytes). Core verifies the signature over
  the first 21,634 bytes with the pinned Arbitrator public identity and uses
  the exact `1000000000` dissemination threshold. The project verifies these
  conditions with the production K12/FourQ provider before printing
  `AUTHORIZED`; a public observation alone is not authorization.
- **Observable identity format:** the clean-room 60-character identity helper
  follows the public base-26/4-chunk plus K12 checksum behavior documented in
  the Qubic CLI key utility:
  https://github.com/qubic/qubic-cli/blob/main/key_utils.cpp . No Qubic CLI or
  core source was copied into this repository, and no private scalar is
  accepted by the helper.
- **Task cache:**
  `https://raw.githubusercontent.com/qubic/core/a83f935406cd006b5b1a94971139e74d410ecb6d/data/bpp9000.task`
  is cached only after exact size 44,744 and SHA-256
  `0c5e9e42c6d86c320af62f4125ca85b2446f2b098893fd6521bcf66c22f7f00a`
  checks. The downloaded bytes are ignored local state and are not committed.

### Source S-012 — Pinned direct-node response correlation and official target behavior

- **Audit date and revision:** 2026-08-10, pinned core
  `a83f935406cd006b5b1a94971139e74d410ecb6d` (`v1.301.3`). The exact local
  source is `/home/umutcagand/qubic-m0.WHEa7H/core`; the source links below
  are immutable GitHub views of the same revision.
- **SystemInfo dejavu semantics:**
  `src/qubic.cpp::processRequestSystemInfo` at lines 1362–1401 fills the
  packed `RespondSystemInfo` payload and calls
  `enqueueResponse(peer, sizeof(respondedSystemInfo), RespondSystemInfo::type(),
  header->dejavu(), &respondedSystemInfo)`. Type 47 therefore echoes the
  exact request `RequestResponseHeader.dejavu`. This handler does not zero it,
  generate a new value, or contain a condition that suppresses the response
  based on the correlation field. The request dispatch is the direct
  `RequestSystemInfo::type()` branch at lines 2086–2089.
- **Comparable response semantics:**
  `processRequestComputors` at lines 1049–1058 enqueues type 2 with
  `header->dejavu()`, and the Entity handler at lines 1253–1284 enqueues type
  32 with the same field. The clean-room client consequently uses exact
  type-plus-dejavu correlation for SystemInfo, Computors, and Entity. A same-
  type wrong-dejavu frame is rejected before asynchronous classification; type
  2 is still an allowlisted unsolicited broadcast type only when it is not the
  desired response for the current request.
- **Handshake order:** `processExchangePublicPeers` at lines 510–541 marks the
  exchange state and consumes the peer list; it does not enqueue a response.
  The new-connection path at lines 7840–7903 queues type 0 immediately. Core
  does not require an incoming client to wait for a peer-exchange response or
  sleep before sending the following request. The implementation therefore
  sends type 0 followed immediately by the requested frame on every fresh
  connection.
- **Dejavu generation and wire representation:** the client uses an atomic
  `uint32_t` counter initialized to 1 and relaxed `fetch_add`; zero is skipped
  on wrap. Frame serialization and parsing use the existing four-byte
  little-endian helpers. Deterministic tests cover nonzero/unique generated
  values, exact response equality, wrong-dejavu SystemInfo/Computors/Entity,
  late/deadline, no-response, and reconnect cases.
- **Observed endpoint behavior:** the official endpoint source remains the
  Qubic Team direct-network article in Source S-009. On 2026-08-10,
  `corenet.qubic.li` returned 43 unique public IPv4 addresses in one DNS
  observation. Some targets accepted TCP and streamed type-0 peer exchange or
  broadcast traffic without returning the requested frame during the bounded
  window. The first implementation selected the resolver's first address on
  every connection, which made standalone and authorization processes
  nondeterministically land on different busy targets. The bounded fix rotates
  the starting address across the current official DNS answer set for fresh
  read-only connections, at most eight attempts within the same 15-second
  absolute deadline. It does not add hard-coded/random third-party peers or
  weaken response validation.
- **Source links:**
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/qubic.cpp#L510-L541,
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/qubic.cpp#L1049-L1058,
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/qubic.cpp#L1253-L1284,
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/qubic.cpp#L1362-L1401,
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/qubic.cpp#L2086-L2089,
  https://github.com/qubic/core/blob/a83f935406cd006b5b1a94971139e74d410ecb6d/src/qubic.cpp#L7840-L7903.
- **Reuse decision:** protocol facts are source-pinned and independently
  reimplemented. No core or Qiner source was copied. The target rotation is
  limited to the official endpoint's live DNS answers and is read-only.

### Source S-013 — Official testnet discovery and raw-endpoint availability

- **Audit date:** 2026-08-10. This source record is a safe M6 preflight only;
  it does not authorize a testnet identity, candidate search, or submission.
- **Current official documentation:** `qubic/docs` commit
  `236365d69ffb8819e9b621e0bc40006175cb1a78` (2026-08-04), path
  `docs/developers/testnet-resources.md`, published at
  https://docs.qubic.org/developers/testnet-resources/. It identifies
  `https://testnet-rpc.qubic.org` as the public testnet node and says dedicated
  projects may receive their own IP/RPC endpoint. It does not publish a raw
  direct-node host/port for general use.
- **Current official core testnet refs:** `qubic/core` ref `testnet` =
  `11625533bfa79fbdc6dd28e9c14455dd1769c749` (2026-07-29),
  `testnets/2026-07-15-bpp9000-mining-algorithm` =
  `d5f95395f25c0769c8d737dab0746c58223518b7`,
  `testnets/2026-07-30-optimize-vote-signing` =
  `29f19e19e438387ae55feb1eb0111ee4e0f966b6`, and
  `testnets/release-301-3` =
  `5be60c894ac2288020887643d269c8adbcf35667`. The current testnet snapshot
  reports version 1.301.0, configured epoch 224, the special BPP9000 threshold
  5,400, the same 44,744-byte task SHA-256
  `0c5e9e42c6d86c320af62f4125ca85b2446f2b098893fd6521bcf66c22f7f00a`,
  and byte-identical header/message-type/SystemInfo/Computors/Entity source
  blobs to the pinned mainnet core revision. Its checked-in
  `knownPublicPeers` is only the localhost placeholder, so it is protocol/task
  evidence, not a public endpoint source.
- **Observed current public service:** `testnet-rpc.qubic.org` resolved to
  Cloudflare edge IPv4 addresses `104.20.18.240` and `172.66.163.101`. TCP
  31841 and 21841 timed out. HTTPS `/` and `/v1/tick-info` returned Cloudflare
  522 during the bounded observation. Even a healthy HTTPS response would be
  RPC evidence only and would not satisfy the direct-node M6 gate.
- **Historical official dedicated-node references:** official
  `qubic/qubic-hackathon` commit
  `ac830bd518b5802010199e7514a55d16d9b0b26f` (2025-08-01) names project-
  specific/shared testnet hosts and raw port 31841. The named dedicated
  example at `185.84.224.158:31841` refused TCP, the deployment example at
  `162.120.18.26:31841` timed out, and the historical shared-testnet host
  `91.210.226.146:31841` accepted TCP but reset all three bounded
  type-0/type-46 attempts before any frame was received. These stale/project-
  specific observations are not a verified current public endpoint.
- **Official local alternative:** `qubic/core-lite` main commit
  `df31a9b0dff195b7b4956fe0601ce83baafea9ef` (2026-08-05),
  https://github.com/qubic/core-lite, documents a self-contained local testnet
  on default raw port 31841 with no initial state files and carries the same
  BPP9000 task SHA-256. Its documented normal local-testnet requirement is 16
  GiB RAM. Long-run mode reports about 32 GiB normally or about 7 GiB with
  `TESTNET_LITE_RAM`, which is explicitly wire/snapshot-incompatible with
  non-LITE nodes. It was investigated but not launched in this checkpoint.
- **Result:** `TESTNET_DIRECT_NODE_NOT_AVAILABLE`. No raw endpoint completed
  SystemInfo, so Computors, Entity, identity creation, authorization,
  candidate work, and submission were not attempted. The full safe evidence
  is `docs/evidence/m6-testnet-preflight.json`.
- **License/reuse decision:** core and Core Lite remain custom Anti-Military
  licensed reference implementations under the existing clean-room rule.
  Official docs/endpoint facts are re-expressed with source links; no upstream
  source, testnet seed, task bytes, or identity material was copied into this
  repository.

## Current Qubic algorithm facts

### Algorithm selection and constants

From core src/mining/score_common.h, Bpp9000 = 1 is the active slot and
Neuraxon = 0 is reserved. From src/public_settings.h and Bpp9000Params:

```text
N=18, M=1, T=8760, W=672, maxTicks=100000,
K=3, P=64, S=100, L=nonce[1] in [1,10],
U=P-N=46, numberWindows=T-W=8088,
default production threshold=3838.
```

A valid/good score is a non-timeout uint32_t no greater than 8,088 and, for
the current default threshold, no greater than 3,838. Runtime system
information carries the threshold; the miner must not use a compile-time
reference threshold as a network truth.

### Task and memory layout

src/mining/task_file.h defines a 96-byte packed header, native little-endian
fixed-width fields, a topology block (N + M + 1 + P*K) * 4 = 848 bytes, and a
packed data block of T * (ceil(N/5) + ceil(M/5)) = 43,800 bytes. The full
production file is 44,744 bytes.

Each data byte packs five base-3 trits. Inputs/outputs/states/LUT entries are
unsigned bytes; topology indices are 32-bit unsigned values. The production
task file SHA-256 is:

```text
0c5e9e42c6d86c320af62f4125ca85b2446f2b098893fd6521bcf66c22f7f00a
```

The header's canonical hashes are:

```text
topology 13e99d5b2fca56aa789cb959575f48392f1a44909a8eaf27f2de8f8d74b07a6b
data     979cdc2247d2ca4ed3d614bf27896384cb1c9c3d804af6ede6b59fc52c0e3dfa
```

The Qiner example file has the same 44,744-byte size but SHA-256
403e24225f5b0512d0cbf49758fed9a01e7334d3cea565ad6c5e82420b713226 and
different content.

### Epoch, seed, and candidate lifecycle

Core src/qubic.cpp calls setNewMiningSeed() at the mining-phase reset. The
expression used to initialize the scorer is
spectrumDigests[(SPECTRUM_CAPACITY*2 - 1)-1]; the source comment states that
one shared random2 pool is used for the epoch and changes between phases. The
seed is 32 bytes and is exposed in the packed RespondSystemInfo message
alongside epoch, tick, and solutionThreshold.

The candidate nonce is 32 bytes. Core requires algorithm byte 1, nonce[1] from
1 through 10, and nonce[2] == 0. Qiner generates random bytes for the
candidate and uses bytes 3..31 in the search hash; core zeroes nonce bytes 0..2
before the candidate search hash. A zero or stale mining seed is rejected
before score acceptance.

The root LUT random draw is derived from KangarooTwelve(publicKey). The
candidate mutation-seed draw is derived from KangarooTwelve(publicKey || nonce)
after zeroing the first three nonce bytes. The root LUT therefore has
public-key reuse; the mutation stream is candidate specific.

### Scoring and mutation hot path

- LUT rows are 27 logical bytes with storage stride 32. The initialization draw
  is 64 * 27 = 1,728 bytes, padded to 1,792. The mutation draw is
  100 * 10 * sizeof(uint64_t) = 8,000 bytes, padded to 8,064.
- Every score starts all 64 states as UNKNOWN (2).
- A non-input neuron uses three prior-tick trits as the base-3 index
  t0 + 3*t1 + 9*t2, then writes the LUT byte to the next state buffer.
  All updates commit simultaneously.
- The scorer feeds 18 input trits for each window while the signal remains
  unknown. After the 672-row training window, it waits for the signal and
  compares output neuron 0 to the target row at index trainingEntryIndex +
  feedCounter. A window that does not settle before 100,000 ticks makes the
  whole score 0xffffffff.
- There are 8,088 windows per score and 101 score calls per candidate
  (initialization plus 100 mutation attempts), for 816,888 window evaluations
  before accounting for the variable tick count.
- A mutation uses delta = seed & 1, flatIndex = (seed >> 1) % 1242, selects
  one of the 46 rows and 27 logical entries, and replaces the old trit with
  (old + 1 + delta) % 3. The step accepts if r <= current; rejected steps
  restore the LUT snapshot.
- Core also contains AVX2/AVX512 window-batched paths (32/64 lanes). They are
  optimized CPU implementations and are not the M1 oracle or evidence that
  XDNA1 is suitable.

### Random2 pool

score_common.h defines:

```text
POOL_VEC_SIZE = (((1ULL << 32) + 64) >> 3) = 536,870,920 bytes
POOL_VEC_PADDING_SIZE = ceil(POOL_VEC_SIZE / 200) * 200
STATE_SIZE = 200 bytes
```

The pool is populated by copying the 32-byte mining seed into a 200-byte
state, zeroing the remainder, applying the 12-round Keccak permutation, and
writing each 200-byte state. random2 requires output sizes divisible by 64. It
reads eight 32-bit little-endian seed words, derives base = (x >> 3) >> 3 and
m = x & 63, extracts/rotates two adjacent pool words, then updates each word
with x = x * 1664525 + 1013904223. This is a large epoch-start memory
operation and a candidate-specific pseudo-random read pattern, not a dense
tensor multiply.

## Direct-node networking specification

### Framing and work acquisition

RequestResponseHeader is 8 bytes: a 3-byte little-endian total message size,
one-byte message type, and four-byte dejavu. Maximum encoded size is 0xffffff;
payload immediately follows the header. The current network enum uses
BROADCAST_MESSAGE = 1, REQUEST_SYSTEM_INFO = 46, and
RESPOND_SYSTEM_INFO = 47.

RespondSystemInfo is a packed 128-byte payload containing version, epoch,
current tick, timing fields, the 32-byte randomMiningSeed, signed
solutionThreshold, and reserved/additional-threshold fields. The direct miner
must refresh this context and stop using work when the epoch/seed changes. The
task file is validated locally; it is not implied by a system-info packet.

### Solution broadcast

BroadcastMessage contains source public key, destination computor public key,
and a 32-byte gamming nonce. The solution payload begins with 32-byte mining
seed, 32-byte nonce, and claimed uint32_t score. The source must be nonzero; the
node verifies the signature over the message, applies source balance or
computor-role dissemination checks, derives the gamming key, reads the
encrypted message type from its first byte, and decrypts the payload. A
solution message is type 0.

Qiner's current direct path opens a configured TCP/IPv4 node connection,
constructs this packet, signs it with the signing key, sends it, and removes
the queued result only after a successful send. It creates a fresh connection
for a submission. This is an implementation reference, not code to copy.
Core recomputes the score with the destination computor key, current mining
seed, and nonce, requires the claimed score to match exactly, requires a
valid/good score, and deduplicates the accepted (nonce, seed, destination)
combination.

An accepted broadcast is later represented as a mining-solution transaction
with mining seed, nonce, score and a security deposit. The node again verifies
the signature, deposit/format, exact score, seed, and threshold before ranking
the miner's best score. The transaction path is source-defined in
src/mining/mining.h and src/qubic.cpp; the protocol markdown is explicitly
draft/incomplete.

### Authentication, reconnect, and compatibility

Direct-node authentication is cryptographic identity and node policy, not a
username/password handshake: the signing public key must verify the packet and
must satisfy source/destination/balance/computor rules. The reference path uses
TCP/IPv4 and reconnects by opening a new configured connection for a submission.
M6 adds the peer-exchange handshake, bounded connect/send/read timeouts, and
finite reconnect attempts around this boundary. The optional production crypto
provider review is complete, but live signing/submission remains explicitly
unavailable without an authorized source identity, destination key, candidate,
and safe runtime secret.

The source revision and task hashes are compatibility inputs. A miner must
reject an unknown algorithm id, invalid task header/hash, unsupported system
info, stale seed, or score threshold context rather than silently assuming
compatibility.

## Pool/network gap

The official pool page describes independently operated pools, and the Qatum
status conflict is recorded above. The QLI client revision S-004 documents one
WebSocket/JWT service but does not expose a complete stable job/share frame
contract. Consequently M0 records:

- direct-node transport and result semantics: source-pinned;
- QLI endpoint/auth configuration: observed documentation only;
- canonical pool job message, share message, framing, version negotiation,
  reconnect behavior, and task distribution: **not pinned**;
- M6 direct-node implementation and validation: required first path;
- Qatum/pool implementation: optional and deferred until a stable authoritative
  protocol or sufficiently complete implementation is pinned and independently
  reviewed.

This is not an M0 blocker because the direct-node path is canonical and its
behavior is source-pinned. No Qatum packet or performance behavior has been
inferred from the conflicting status statements, forums, or old articles.

## License audit and reuse policy

| Project | License / SPDX | Can target reuse source? | Required action |
|---|---|---|---|
| qubic/core | Custom Anti-Military; no SPDX; narrow uint128 MIT exception | No assumption; scorer/node are reference-only | Clean-room implementation; preserve no source structure; legal review for any copy |
| qubic/Qiner | Custom Anti-Military; no SPDX; Catch2 BSL-1.0 in notice | No assumption | Clean-room implementation; no source copy; notices/field restrictions would apply to permitted copies |
| qubic-li/client | No license file/SPDX found | No | Do not copy code or undocumented protocol |
| Official Qubic docs | Documentation, no code reuse decision | Protocol facts may be re-expressed | Link/cite source; implement independently |
| hawkpoint-npu-llm | Project Apache-2.0/LLVM notices; optional MIT component | Not coupled or copied | Use as environment reference only |
| Xilinx/mlir-aie | Apache-2.0/LLVM exceptions as applicable | Dependency candidate only | M2 performs exact dependency/notice review |

The target project's own license is still undecided. The safe M0 rule is
clean-room reimplementation of Qubic algorithm/protocol behavior, with source
and license notices kept in this registry and no upstream code imported.

## Reproducibility commands

The exact audit can be recreated without trusting this document's summaries:

```bash
git clone https://github.com/qubic/core.git
git -C core checkout a83f935406cd006b5b1a94971139e74d410ecb6d
git clone https://github.com/qubic/Qiner.git
git -C Qiner checkout 11fb18a6f4944bb55fe103d3f263cb5d31e00200
git clone https://github.com/qubic-li/client.git
git -C client checkout 9a01902342240c69b19d9cceb637ea68916a3d2c

git -C core show --stat --oneline a83f935406cd006b5b1a94971139e74d410ecb6d
git -C Qiner show --stat --oneline 11fb18a6f4944bb55fe103d3f263cb5d31e00200
sha256sum core/data/bpp9000.task Qiner/data/example_task_bpp9000.bin
```

Expected task-file SHA-256 values are recorded above. The main branches and
tags were observed at the pinned commits on the audit date; future updates
must create a new source record and re-run the discrepancy/license review.

## M0 conclusions

- Current active algorithm and exact score dimensions are verified from core and
  Qiner at pinned revisions.
- Direct-node seed/epoch/task/threshold/result lifecycle is understood at
  specification level.
- The recurrent LUT tick is the only high-suitability initial XDNA1 candidate;
  fused window/candidate batching remains a benchmark hypothesis.
- CPU owns control, freshness, crypto, mutation policy, and canonical
  verification.
- No benchmark value has been claimed.
- Pool wire compatibility remains an optional future adapter gate because the
  official sources do not provide a sufficiently complete, consistent wire
  specification. It is not an M0 or M1–M5 dependency.
# Pearl active upstream note

The active Pearl source remains `pearl-research-labs/pearl` master at
`fe22b6a2b831d95b2f56564808f39d2f498f34a5`; the official local gateway is
newline-delimited JSON-RPC over `/tmp/pearlgw.sock` or loopback TCP 8337 with
`getMiningInfo` and `submitPlainProof`. The repository's clean-room client
implements those exact boundaries and the node's `getblocktemplate`/
`submitblock` path through a credential-safe HTTP adapter. No Stratum protocol
was found or implemented (`POOL_PROTOCOL_UNAVAILABLE`).

The MLIR-AIE dependency used for the project-owned P2 kernel was observed at
commit `57d7494e99c214f5f53b328a0ed43a99e759e835`; the host runtime observed
amdxdna rc7 while the historical pinned stack remains recorded unchanged.
The Pearl hot miner/gateway/proof components still have no clear local reuse
grant, so their source is not copied. P7 live interoperability is blocked by
the absence of an installed official gateway/prover/node, not by an invented
protocol workaround.

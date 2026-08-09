# Upstream Sources and Licensing

Audit date: **2026-08-09**.

This registry is the source of truth for M0's current Qubic claims. Source code
was inspected in separate checkouts under /home/umutcagand/qubic-m0.WHEa7H; none
of that source was copied into this repository.

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
- **Reuse decision:** Reference/dependency candidate, not yet a target
  dependency. M2 must reproduce the environment check and make an explicit
  dependency decision.

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
A future implementation must add bounded connect/send/read timeouts and
classify rejection; M0 does not implement it.

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

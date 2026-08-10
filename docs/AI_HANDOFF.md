# AI Handoff

This file is the authoritative short-form state for the next zero-context
engineering agent.

## Active track

**Pearl (PRL) P1 — trusted clean-room CPU golden path and canonical vectors**

The active Pearl handoff is [`docs/pearl/AI_HANDOFF.md`](pearl/AI_HANDOFF.md).
P1 is CPU-only: the independent oracle and canonical corpus are implemented,
but no Pearl NPU, node/pool, wallet, live mining, submission, or ZK proof has
been run. The active branch is `feat/pearl-p1-cpu-golden`, starting at
`a15ed125295cc4361425a4b11159aa5744f3f160`. P1 implementation commit
`4d91323f0a22e2f03b82d34fbf84791fa5bd83d5` and the final evidence digest are
recorded in the Pearl handoff.

## Frozen Qubic reference milestone

**M6 — Qubic direct-node integration**

## Status

**IN PROGRESS** — the direct-node boundary and the optional pinned production
K12/FourQ provider are implemented and offline/KAT tested. The bounded public
read-only system-info probe passes against the official direct-node endpoint,
including the peer-exchange handshake and reconnect/context refresh. The
current computor-list and pinned production-task sources are now verified, but
live submission remains unexercised because no authorized source identity,
eligible candidate, or runtime signing secret was available. A recovered safe
testnet preflight found no currently verifiable official public raw direct-node
endpoint; only the official HTTPS RPC and source-pinned local/dedicated-node
alternatives are documented. Do not start M7 or Qatum work.

## Current M6 safe-testnet preflight checkpoint — 2026-08-10

Evidence/documentation commit:
`4e1259738a02e76d4048a5c9d157bf43989b379b` (`m6: record safe testnet
direct-node preflight`). `origin/main` remains at the recovered
`ac3714c6b7069e5957b7d547341317f580fdc6b4`; the evidence commit and this
follow-up handoff are local checkpoints and were not pushed.

Recovery started from clean `main` at
`ac3714c6b7069e5957b7d547341317f580fdc6b4`, exactly matching
`origin/main`. There were no staged, unstaged, or untracked files and no
interrupted miner/build/test process. Both build trees were present.
`git fsck --full --no-reflogs` found no missing or corrupt object; its only
dangling commit was an older M0 documentation snapshot, not interrupted
testnet work. No reset, clean, restore, rebase, or secret/cache deletion was
performed.

The prior testnet task had not reached a checkpoint. Before this continuation
there was no `docs/evidence/m6-testnet-preflight.json`, testnet configuration,
testnet identity, candidate runner, or testnet commit. Metadata-only inspection
showed exactly one regular file under `.local-secrets/`, at the existing
default mainnet identity path, and no second/testnet-named identity. Its
contents were not opened or printed. `.local-cache/` was also preserved.

### Official discovery and bounded endpoint result

- Current Qubic documentation at `qubic/docs`
  `236365d69ffb8819e9b621e0bc40006175cb1a78` publishes only the HTTPS RPC
  endpoint `https://testnet-rpc.qubic.org`. It says project-specific dedicated
  nodes may be supplied with their own IP/RPC endpoint; it publishes no raw
  direct-node host/port for general use.
- Current official core testnet refs were fetched and inspected without
  copying source. `testnet` is
  `11625533bfa79fbdc6dd28e9c14455dd1769c749`; the current BPP9000 test branch
  is `d5f95395f25c0769c8d737dab0746c58223518b7`; and
  `testnets/release-301-3` is
  `5be60c894ac2288020887643d269c8adbcf35667`. The testnet snapshot uses the
  same 44,744-byte BPP9000 task SHA-256 as the pinned mainnet source, protocol
  schemas match the pinned mainnet revision, and its special threshold is
  5,400. Its checked-in public-peer configuration contains only the localhost
  placeholder, not a public direct endpoint.
- The current RPC hostname resolved only to Cloudflare edge addresses. TCP
  31841 and 21841 timed out; HTTPS `/` and `/v1/tick-info` returned 522 during
  this checkpoint. RPC availability would not count as raw direct-node proof
  even if HTTP recovered.
- The older official `qubic/qubic-hackathon` checkpoint
  `ac830bd518b5802010199e7514a55d16d9b0b26f` contains historical dedicated
  and shared testnet examples. `185.84.224.158:31841` refused TCP and the
  documentation example `162.120.18.26:31841` timed out. TCP to the historical
  shared-testnet host `91.210.226.146:31841` opened, but all three bounded
  clean-room attempts were reset after the type-0 handshake/type-46 request
  and before any response frame: zero ignored frames/bytes, 904 ms total. It
  is not a verified endpoint.

The authoritative outcome is therefore:

```text
TESTNET_DIRECT_NODE_NOT_AVAILABLE
verified_raw_direct_node_endpoint=NONE
system_info=FAIL_NO_RESPONSE_FRAME
computors=NOT_ATTEMPTED_SYSTEM_INFO_GATE_FAILED
entity=NOT_ATTEMPTED_SYSTEM_INFO_GATE_FAILED
candidate_runner=NOT_STARTED
submission_performed=false
```

Every testnet protocol request set `XDNA_QUBIC_NETWORK=testnet`, disabled live
submission, and explicitly removed signing environment variables. The current
runtime does not yet enforce that network label, so no identity-capable command
was used. The mainnet identity was not read, replaced, funded, or used. No
testnet seed or identity was created, no candidate/NPU/CPU score was produced,
and no solution frame was built or sent.

### Official alternative and exact continuation

Official Core Lite at
`df31a9b0dff195b7b4956fe0601ce83baafea9ef` documents a self-contained local
testnet on raw port 31841 and carries the same pinned BPP9000 task. It was not
launched. Its normal local-testnet requirement is 16 GiB RAM while this host
has 14 GiB physical / 6.2 GiB currently available; the documented ~7-GiB
`TESTNET_LITE_RAM` mode is explicitly wire/snapshot-incompatible with non-LITE
nodes. Treat local provisioning as a separate bounded compatibility/resource
task, not an automatic substitute for a public interoperability run.

Files changed in this checkpoint are the testnet evidence record and the M6
documentation listed by the final Git diff. No source, CMake, M5 backend,
identity tool, Qatum, or M7 file changed. No physical NPU run was repeated;
the inherited M1–M5 evidence remains authoritative because accelerator
semantics did not change.

Final verification:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug                         PASS
cmake --build build -j2                                              PASS
ctest --test-dir build --output-on-failure                           PASS 6/6
cmake -S . -B build-crypto -DCMAKE_BUILD_TYPE=Debug
  -DXDNA_ENABLE_PRODUCTION_CRYPTO=ON                                 PASS
cmake --build build-crypto -j2                                       PASS
ctest --test-dir build-crypto --output-on-failure                    PASS 7/7
./scripts/test-m6-local-identity.sh                                  PASS
python3 -m json.tool docs/evidence/m6-direct-node.json               PASS
python3 -m json.tool docs/evidence/m6-testnet-preflight.json         PASS
git diff --check                                                     PASS
```

The exact next task is one of these official-only paths: obtain a current raw
testnet endpoint from Qubic developers, or explicitly approve and provision
the pinned Core Lite local testnet after its resource/wire compatibility is
reviewed. Before creating any testnet identity, add an enforced network enum,
testnet-specific Arbitrator/task policy, and isolated testnet secret/cache
paths. Then prove SystemInfo -> signed Computors -> Entity in read-only mode.
Do not create or use an identity, implement the candidate runner, or attempt a
testnet submission until those three stages pass. Mainnet remains separately
blocked by `MAINNET_IDENTITY_NOT_AUTHORIZED`; M6 remains **IN PROGRESS**.

## Current focused M6 read-only authorization checkpoint — 2026-08-10

Implementation commit: `6a5f25fcdc35aee3b5a352f3e93041d13bf4252b`.

The remaining authorization-query failure is isolated and fixed at the
read-only endpoint policy boundary. It was not a SystemInfo dejavu bug or a
handshake-order race. `corenet.qubic.li` currently resolves to many official
IPv4 direct-node targets (43 unique addresses observed in this session). Some
targets accept TCP and stream peer/broadcast traffic but do not return the
requested response within the bounded request. The old standalone script also
used only two reconnect attempts, while the authorization path used its old
three-attempt default. Separate processes could therefore select different
resolver-ordered targets. The old authorization failure was exactly this
behavior: SystemInfo reached the 15,000-ms absolute deadline after 5,220
ignored frames / 3,423,184 bytes.

The fix keeps the 15,000-ms deadline and all existing framing/resource
hardening. Each fresh read-only connection rotates the starting address in the
current official hostname resolution, with a finite maximum of eight bounded
attempts. No public IP is hard-coded, no third-party peer is used, and the
submission path is not broadened. The standalone live script now uses the same
eight-attempt policy. Some official targets remain load-variable; when all
targets selected within the deadline are unresponsive, the result remains
`CHECK_UNAVAILABLE` rather than weakening response validation.

### Pinned-core protocol facts

- In core revision `a83f935406cd006b5b1a94971139e74d410ecb6d`,
  `src/qubic.cpp:1362-1401` constructs type-47 `RespondSystemInfo` and calls
  `enqueueResponse(..., header->dejavu(), ...)`. SystemInfo therefore echoes
  the request dejavu exactly; it does not set zero, generate a new value, or
  suppress the response under a hidden correlation condition. Dispatch is the
  direct `REQUEST_SYSTEM_INFO` case at `src/qubic.cpp:2086-2089`.
- `processExchangePublicPeers` at `src/qubic.cpp:510-541` consumes the type-0
  peer exchange and marks state without enqueueing a reply. The core's new
  connection loop sends type 0 immediately; it does not require the client to
  wait for a peer-exchange response before sending its request. The client
  therefore keeps the deterministic type-0-then-request order without a sleep.
- Core Computors type 2 and Entity type 32 handlers also pass the request
  dejavu unchanged. The reader checks desired response type first, accepts
  only exact dejavu equality, fails closed on same-type wrong dejavu, and only
  then consults the asynchronous allowlist. Type 2 remains allowlisted for
  unsolicited broadcast traffic but cannot swallow a desired type-2 response.
- `next_dejavu()` uses an atomic `uint32_t` counter initialized at 1, relaxed
  `fetch_add`, skips zero on wrap, and is serialized/read in the existing
  little-endian frame helpers. Deterministic tests cover nonzero, unique,
  exact request/response correlation and wrong-dejavu boundaries.

### Controlled matrix and final live state

The opt-in aggregate diagnostic matrix covers A–H: standalone SystemInfo,
authorization SystemInfo stage, each single stop, both two-query orders, and
the complete SystemInfo → Computors → Entity sequence. The primary post-fix
three-repeat run accepted all 36 query steps. An additional three-repeat load
observation accepted 34/36; the two failures were A/B SystemInfo attempts that
received only five type-0 frames before the same deadline. Across that
observation the ignored-frame distribution was type 3: 11,146; type 0: 43;
type 24: 42; types 8 and 29: 20 and 43; type 14: 20; type 16: 3; and types 1
and 68: 1 each. No payloads or signing bytes are logged.

The complete live authorization check now succeeds through all three trusted
read-only stages and returns:

```text
NOT_AUTHORIZED
system_epoch=225
computors_epoch=225
computors_signature_verified=true
entity_incoming_amount=0
entity_outgoing_amount=0
entity_energy=0
required_threshold=1000000000
source_is_current_computor=false
submission_performed=false
```

Standalone SystemInfo also passed twice at epoch 225, threshold 3838, with
ticks 73338141 and 73338143. Computors passed with epoch 225, a 21,698-byte
response, 676 nonzero keys, and key-list SHA-256
`58ef30a7fece845226c91502ff616747e1d50aab34ef530e68e15a36231aa9bf`.
The existing local identity was not replaced or funded; its signing subseed
was never printed. No candidate search, submission, M7, or Qatum work was
started. The exact next task is to stop at this trusted read-only decision.

All older checkpoint sections below are historical records. Their continuation
templates for identity generation, funding, candidate search, or submission
must not be followed for this focused task.

## Historical M6 direct-node demultiplexing checkpoint — before the focused fix

- Branch: `main`; M6 remains **IN PROGRESS**. The existing ignored local
  operator subseed was preserved; `setup-m6-local-identity.sh` was not run,
  no identity was replaced, and no secret was printed, committed, funded, or
  used for submission.
- Root cause of the old false `NOT_AUTHORIZED`: `request_read_only()` treated
  64 valid asynchronous frames as a failure. The public stream delivered more
  than that before a requested reply, so this was transport incompleteness—not
  an authoritative authorization decision.
- The reader now uses one absolute `steady_clock` deadline per request across
  all reconnects (default 15,000 ms), caps each socket read **and write** to
  the remaining deadline and configured 3,000-ms timeout, and allows only known
  asynchronous Qubic message types. It also has finite 16-MiB ignored-byte
  and 8,192-frame defensive ceilings; hitting either ceiling is terminal and
  cannot be bypassed by reconnecting. Valid broadcasts never extend the
  deadline. Type-2/32 responses require the current official core's echoed
  request `dejavu`; system-info follows the same request/response rule.
- Offline `qubic_direct_node_tests` passes a response after **200** valid
  `BROADCAST_TICK` frames plus immediate-response, byte/frame ceiling,
  deadline/read-timeout, unknown-frame, malformed-frame, close, and reconnect
  paths. The production and default CTest suites remain green.
- Live after the change: `run-m6-live-system-info.sh` passed twice (epoch 225,
  ticks 73321785/73321793, threshold 3838); `run-m6-live-computors.sh` passed
  with epoch 225, exact 21,698-byte payload and 676 nonzero keys after 96
  ignored frames / 34,224 ignored bytes in 779 ms. The local authorization
  gate stayed **CHECK_UNAVAILABLE**, correctly at `stage=system_info`,
  `reason=request_deadline_exceeded` after 5,220 ignored frames / 3,423,184
  ignored bytes / 15,005 ms. It is not `NOT_AUTHORIZED`; entity state and
  energy were not authoritatively obtained.
- Superseded by the focused read-only authorization checkpoint above. The
  endpoint behavior described here remains useful as the pre-fix reproduction;
  the final trusted result is recorded above and in the evidence JSON.

## Latest M6 secure-identity checkpoint — 2026-08-10

- Branch: `main`; implementation commit `fb40336`
  (`m6: add secure local identity authorization workflow`) is recorded and
  will be pushed with this handoff checkpoint. M6 remains **IN PROGRESS**. No persistent local
  identity was generated, no signing secret was printed or committed, and no
  funds or submission frame were used.
- Added `m6_identity_tool` with Linux CSPRNG generation, strict current-user
  0600 file loading, 0700 directory workflow, clean-room public identity
  encode/decode, overwrite refusal, and explicit secure erase. The safety
  script uses only temporary identities and verifies secret absence from
  stdout, stderr, evidence, and diff captures.
- Added strict official type-11/type-2 computor and type-31/type-32 entity
  parsers/requests. The current list is 21,698 bytes; the entity response is
  840 bytes. `m6_authorization_check` verifies list epoch, nonzero keys,
  pinned Arbitrator signature, and the official current-computor or
  `incomingAmount - outgoingAmount >= 1000000000` entity rule. It prints
  `AUTHORIZED` or `NOT_AUTHORIZED` only; it never submits.
- Added the pinned task fetch/cache and K12 parser path. The cache uses the
  core revision `a83f935406cd006b5b1a94971139e74d410ecb6d`, exact 44,744-byte
  size, and SHA-256
  `0c5e9e42c6d86c320af62f4125ca85b2446f2b098893fd6521bcf66c22f7f00a`.
  The ignored local cache was fetched and parsed successfully in this
  session; task bytes are not in Git.
- Added `scripts/run-m6-final-live-submit.sh`. It requires explicit opt-in,
  runs the production KAT and authorization gate first, and stops before any
  candidate search because the production random2/candidate-orchestration
  runner is not wired. It reports zero candidate/NPU/CPU/frame counts and
  does not fabricate a WorkContext, score, signature, retry, or send.
- Verification so far: production build and CTest `7/7`, direct-node parser
  tests, identity safety test, shell syntax checks, pinned task fetch and
  `m6_task_verify` all pass. A temporary read-only authorization probe was
  attempted against `corenet.qubic.li:21841`; it returned `NOT_AUTHORIZED`
  with an external protocol/transport failure before a complete decision.

### Exact continuation commands

```bash
cd /home/umutcagand/xdna-npu-miner
cmake --build build-crypto -j2
ctest --test-dir build-crypto --output-on-failure
./scripts/test-m6-local-identity.sh
python3 -m json.tool docs/evidence/m6-direct-node.json
git diff --check
```

Do not generate an operator identity, request funding, paste a secret into
chat, start M7/Qatum, or claim M6 complete. The next implementation task is
only the bounded production random2/candidate runner after legitimate local
authorization is available; until then preserve the fail-closed no-send
state.

## Latest M6 authorization/data checkpoint — 2026-08-10

- Current official remote tips were revalidated with `git ls-remote`: core
  `main`/`v1.301.3` =
  `a83f935406cd006b5b1a94971139e74d410ecb6d`; Qiner
  `main`/`v1.302.3` =
  `11fb18a6f4944bb55fe103d3f263cb5d31e00200`. The exact current source
  conditions are recorded in Source S-010 of `docs/UPSTREAM.md`.
- Core authorization is exact: the source public key must be nonzero and the
  BroadcastMessage signature must verify. A source is dissemination-authorized
  when it is a current computor or when it is a spectrum entity with
  `energy(source) >= 1000000000`; for a non-computor direct destination the
  same balance condition is required. The destination must be a current
  computor public key. No source credential was inferred or fabricated.
- `./scripts/run-m6-live-computors.sh` completed a bounded read-only
  `REQUEST_COMPUTORS` probe at `2026-08-10T06:13:37Z` against
  `corenet.qubic.li:21841`: response type 2, 21698-byte payload, epoch 225,
  676 nonzero keys, key-list SHA-256
  `58ef30a7fece845226c91502ff616747e1d50aab34ef530e68e15a36231aa9bf`.
  The live system-info refresh at 06:08:24Z–06:08:25Z was also epoch 225,
  threshold 3838, and 8088 windows. No key was hard-coded or selected for an
  unsigned attempt; the probe does not claim Arbitrator-signature verification.
- The authoritative production task is the official core
  `data/bpp9000.task` at the same core revision: 44744 bytes, file SHA-256
  `0c5e9e42c6d86c320af62f4125ca85b2446f2b098893fd6521bcf66c22f7f00a`, shape
  `N18 M1 T8760 W672 P64 K3 S100`, and the core-pinned topology/data hashes.
  Core loads and K12-verifies this file at node initialization. SystemInfo
  does not carry task bytes; the task remains an external clean-room input and
  is not copied into this repository. The project has not claimed a live task
  payload or run its parser against an upstream checkout in this checkpoint.
- Official Qubic CLI/wallet documentation provides identity creation,
  `-showkeys`, `-getbalance`, and `-sendtoaddress`, but a fresh identity is
  not enough to pass core's dissemination rule. Satisfying the minimum would
  require user-owned funds and a local signing seed, or an actual current
  computor identity and its secret. No such runtime configuration exists
  (`XDNA_QUBIC_SIGNING_PUBLIC_KEY_HEX`/`...SECRET_HEX` absent); no funds were
  spent and no secret was requested in chat.
- Submission evidence is therefore still
  `LIVE_SUBMISSION_NOT_EXERCISED_PROTOCOL_REQUIRES_AUTHORIZED_IDENTITY`:
  attempts 0, NPU evaluations 0, CPU comparisons 0, best finite score unset,
  threshold 3838, stale aborts 0, frame sent false, acknowledgement not
  applicable. TCP write is not an acceptance and no result classification from
  the four live-send outcomes is claimed.
- Exact next authorized-only command template uses the explicit local secret
  file and never places a signing value in the environment or chat:

  ```bash
  ./scripts/setup-m6-local-identity.sh
  ./scripts/check-m6-local-authorization.sh
  XDNA_QUBIC_ALLOW_LIVE_SUBMISSION=1 ./scripts/run-m6-final-live-submit.sh
  ```

  In the current checkout the final command remains deliberately guarded and
  stops before candidate search because the production random2/candidate
  runner is not wired; it is not evidence of a send. Do not request funding,
  paste a secret, or turn the command into a supervisor or retry loop.

## Final M6 checkpoint for this session — 2026-08-10

- Branch: `main`. The gate/evidence commit is
  `028e5a2020653861dd72ad2fb16043c9a9abaf4b` (`m6: record current
  authorization data gate`). The follow-up handoff edit is the only later
  change and must remain on this branch.
- Files changed in the gate commit: `docs/AI_HANDOFF.md`,
  `docs/ARCHITECTURE.md`, `docs/DECISIONS.md`, `docs/TESTING.md`,
  `docs/UPSTREAM.md`, `docs/evidence/m6-direct-node.json`,
  `scripts/run-m6-live-computors.sh`, and
  `scripts/run-m6-live-submit.sh`.
- Verification completed: default CMake build and CTest `6/6` passed;
  production-crypto CMake build and CTest `7/7` passed; corpus generation
  reproduced `2979889feed3352b3c12831a301a357b6c9099f3de80b955f152c53bca2f8c03`
  and `7c1da1028b9ecdbae54616654606185e62076ff7b69e209ecbf3d23f6a2fede1`;
  offline live-probe tests passed; both guarded submission invocations
  exited 2 without sending; JSON, shell syntax, diff-check, and secret scan
  passed.
- Live read-only checks completed: SystemInfo passed with epoch 225,
  threshold 3838, nonzero seed, and 8088 windows; the current-computor probe
  passed at `2026-08-10T06:19:39Z` with a 21698-byte type-2 response, 676
  nonzero keys, and key-list digest
  `58ef30a7fece845226c91502ff616747e1d50aab34ef530e68e15a36231aa9bf`.
  No physical NPU run was repeated in this documentation-only checkpoint;
  inherited M1–M5 hardware evidence remains the verified record.
- Known blocker: no user-authorized funded spectrum identity, current
  computor secret, or local signing subseed exists. Therefore no candidate
  was scored, no CPU/NPU candidate comparison occurred, no frame was built or
  sent, and no acknowledgement/acceptance claim exists. No secret or funds
  were used.
- Do not redo the completed M1–M5 physical gates, crypto KATs, or public
  read-only probes without a source/toolchain change. Do not start M7 or
  Qatum. The next exact task is either to preserve the blocked M6 state or,
  after legitimate local authorization is available, add only the minimum
  bounded one-shot task/candidate/submission glue and classify the actual
  live result; never paste signing material into chat.

## Current continuation checkpoint — 2026-08-09

- Branch: `main`; the pre-change recovery checkpoint
  `9e5342f8611c65fa8b249500b15d9bd563e6da1` was pushed and matched
  `origin/main` before this continuation.
- Added the optional `K12FourQCryptoProvider` in
  `src/qubic/production_crypto.*`, pinned to independently reviewed
  FourQlib/K12 sources recorded as Source S-008 in `docs/UPSTREAM.md`.
- Added exact RFC 9861 K12, synthetic Qubic SchnorrQ, FourQ public-key/shared-
  key, gamming-key, and 68-byte gamma-stream KATs in
  `tests/qubic_crypto_tests.cpp`; no real user secrets are present.
- Default build remains dependency-free with crypto disabled. The opt-in
  build flag is `-DXDNA_ENABLE_PRODUCTION_CRYPTO=ON`.
- Current status is **IN PROGRESS**: provider/KAT gate passed; live endpoint,
  and read-only live system-info gate passed; live submission remains
  **NOT_EXERCISED**.

## Power-off recovery checkpoint — 2026-08-10

- The requested recovery was performed before editing. The interrupted work
  was found uncommitted on `main` at `23e9a0f51488cae8eb69e7fdff97ab2839c6b6eb`;
  `origin/main` is the same commit and the worktree is dirty with no staged
  changes.
- `git fetch origin` completed. `git fsck --full` reported six dangling
  objects (three trees and three blobs) and no missing or corrupt objects.
  No reset, clean, restore, checkout, rebase, or pull was performed.
- Recovered uncommitted M6 files include the peer-exchange changes in
  `src/qubic/direct_node.*`, `src/qubic/live_probe_main.cpp`,
  `tests/qubic_direct_node_tests.cpp`, `CMakeLists.txt`, the three M6 scripts,
  `docs/evidence/m6-direct-node.json`, and the related M6 documentation.
  An unrelated regenerated M2 artifact UUID was inspected and restored to the
  committed verified value in `docs/evidence/m2-xdna-smoke.json`; no M2
  dispatch claim was changed or folded into this M6 checkpoint.
- Recovery classification: official endpoint/core/Qiner source revalidation
  is complete; the live probe, scripts, mock/reconnect path, public endpoint,
  and live context construction are implemented but require this session's
  targeted build/offline/live verification. Evidence is present but
  uncommitted. Ephemeral identity and actual live submission are intentionally
  not started because current protocol authorization material is absent.
- Exact next task: build the recovered M6 surface, run the offline probe and
  focused CTest/JSON checks, then rerun the bounded read-only official endpoint
  probe. Preserve the guarded no-send submission status; do not start M7 or
  Qatum work.

## M6 live interoperability checkpoint — 2026-08-10

- Branch: `main`; public endpoint source is the official Qubic Team article
  documenting `corenet.qubic.li:21841`.
- `./scripts/run-m6-live-system-info.sh` passed twice on 2026-08-10T05:48:42Z
  to 2026-08-10T05:48:43Z UTC. The adapter sent the type-0 peer-exchange
  handshake and type-46 request, received type-47/136-byte frames with
  128-byte payloads, and refreshed the context from epoch 225, tick 73296942
  to 73296943, with threshold 3838 and 8088 work windows. The mining seed was
  nonzero; only a short diagnostic fingerprint is recorded.
- `./scripts/test-qubic-live-probe-offline.sh` passed success, wrong-frame,
  truncated-frame, and timeout behavior; `qubic_direct_node_tests` also passed
  one-byte fragmented reads and bounded reconnect. The guarded submission
  script was tested both without opt-in and with explicit opt-in; both paths
  exited nonzero without sending a frame.
- The live response contains no task bytes or destination computor public key.
  The recorded task identity was used only to construct the local context;
  full live task compatibility is explicitly **NOT_PROVEN**.
- Submission status is
  `LIVE_SUBMISSION_NOT_EXERCISED_PROTOCOL_REQUIRES_AUTHORIZED_IDENTITY`:
  candidate attempts 0, CPU verification false, NPU verification false,
  frame sent false, ephemeral identity false, real user secret false. The
  exact external requirement is an authorized nonzero source satisfying the
  current computor/dissemination-balance rule, a current computor destination,
  current task-compatible work, an eligible CPU/NPU-verified candidate, and a
  safe runtime secret. Do not invent any of these.
- Files changed in this continuation: `CMakeLists.txt`,
  `src/qubic/direct_node.hpp`, `src/qubic/direct_node.cpp`,
  `src/qubic/live_probe_main.cpp`, `tests/qubic_direct_node_tests.cpp`,
  `scripts/run-m6-live-system-info.sh`, `scripts/run-m6-live-submit.sh`,
  `scripts/test-qubic-live-probe-offline.sh`, and the M6 documentation/evidence
  files listed in the final commit.

## Final recovery validation — 2026-08-10

- Branch: `main`; implementation commit: `d4268e1` (`m6: add live
  system-info interoperability probe`). The worktree was clean immediately
  after that commit.
- Default verification passed:
  `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`,
  `cmake --build build -j2`, and
  `ctest --test-dir build --output-on-failure` = **6/6**.
- Production crypto verification passed:
  `cmake -S . -B build-crypto -DCMAKE_BUILD_TYPE=Debug
  -DXDNA_ENABLE_PRODUCTION_CRYPTO=ON`, build, and
  `ctest --test-dir build-crypto --output-on-failure` = **7/7**.
  `./scripts/generate_corpus.sh build` reproduced 100 generated and 10
  production-shaped cases with the recorded digests.
- `./scripts/test-qubic-live-probe-offline.sh` passed success with injected
  unsolicited peer/broadcast frames, wrong-frame, truncated-frame, and
  timeout cases. The direct-node CTest covered one-byte fragmented reads,
  bounded reconnect, all no-send gates, and deterministic mock serialization.
- `./scripts/run-m6-live-system-info.sh` passed twice against the official
  `corenet.qubic.li:21841` endpoint at 2026-08-10T05:48:42Z–05:48:43Z UTC:
  epoch 225, ticks 73296942–73296943, threshold 3838, 8088 windows, version
  301, nonzero seed, type-47/136-byte response, and reconnect/context advance.
  `python3 -m json.tool` passed for the M6 and M5 evidence records and
  `git diff --check` passed.
- `scripts/run-m6-live-submit.sh` exited 2 without opt-in and with explicit
  `XDNA_QUBIC_ALLOW_LIVE_SUBMISSION=1`; both paths sent no frame. The secret
  scan found no private-key material or real signing secret. M1–M5 physical
  evidence remains the previously recorded verified result; those expensive
  hardware regressions were not rerun because this M6 change did not alter
  accelerator semantics.
- Known limitation: the live system-info response contains neither task bytes
  nor a destination computor key. No authorized source identity, current
  task-compatible candidate, or runtime signing secret exists, so live
  submission remains **NOT_EXERCISED** and M6 remains **IN PROGRESS**.

## Crash recovery checkpoint

- Recovery date: 2026-08-09.
- Branch: `main`.
- Recovery HEAD before M6 work: `62a84d5d8674a1a74c1b7348e1fa41c85348e026`
  (`record M5 handoff commit`), also `origin/main`.
- Original M6 checkpoint commit: `c600094` (`m6: add direct-node protocol
  boundary`).
- M6 post-recovery checkpoints: `75e0a78` and `9e5342f` (pushed before this
  continuation).
- The current continuation regenerated the M2–M5 evidence records and added
  the optional production-crypto source/tests/docs; the implementation and
  evidence are committed in `8ef85c2`.
- No source or evidence files remain staged before this handoff finalization.
- Recovered commits: the M0–M5 commit chain through `62a84d5`; the original
  recovered M6 checkpoint was `c600094`, followed by the post-recovery
  validation checkpoint `9e5342f` and the production-crypto/KAT checkpoint
  `8ef85c2`.
- `git fsck --full`: five dangling objects (two trees and three blobs) only;
  no missing or corrupt object was reported. No conflict markers, editor
  temporary files, or M6 files were found.
- The four inherited evidence diffs were regenerated by the post-recovery
  M2/M3/M4/M5 validation commands, parse successfully, and are ready for the
  next focused evidence checkpoint. They were not discarded or normalized.
- First incomplete M6 task after this continuation: obtain explicit endpoint
  and signing-material authorization, then perform only the bounded live
  system-info/submission gate if those materials are safe and current.

## Branch and commits

- Branch: `main`
- M0 completion commit:
  `057ee66c679a7ff89c1b90abefb72384184159e5`
- M1 implementation/checkpoint commit:
  `323e2bcdc6885ceb4c6ec3ce65af7e651b3e85bb`
- M1 handoff commit:
  `749311c16bf40604aab7521625a58f859e6a9d75`
- M2 completion commit:
  `4ae226a048a65fed67fd7b8ab6a8feee9ec4c696`
- M3 implementation commit:
  `f5836e2fb0fd57d03babe6c3c3647db06fd0c269`
- M4 implementation/evidence commit: `c6308b1`
- M5 implementation/evidence commit: `bd9a349`
- M6 implementation/evidence checkpoint: `c600094`.
- M6 post-recovery validation/evidence checkpoint: `75e0a78`.
- M6 production-crypto/KAT checkpoint: `8ef85c2`.
- M6 live system-info interoperability probe: `d4268e1`.

## M0 authority that M1 used

- Qubic core: `v1.301.3`,
  `a83f935406cd006b5b1a94971139e74d410ecb6d`.
- Qiner reference aid: `v1.302.3`,
  `11fb18a6f4944bb55fe103d3f263cb5d31e00200`.
- Canonical active algorithm: BPP9000 (`nonce[0] == 1`).
- Production shape: `N=18, M=1, T=8760, W=672, P=64, K=3, S=100`.
- Production score windows: `8088`; maximum ticks per window: `100000`.
- Timeout sentinel: `0xffffffff`; lower finite score is better.
- Runtime threshold is supplied by system information; the reference does not
  treat the Qiner example threshold `6469` as production truth.

Do not redo the M0 source/license audit unless a concrete upstream
contradiction affects the CPU semantics. Do not copy Qubic core, Qiner, or QLI
source. Their implementation source is Anti-Military licensed or unlicensed;
M1 is clean-room.

## Completed M1 work

- Added a standalone C++20/CMake library under `src/bpp9000/`.
- Added fixed-width public-key, mining-seed, nonce, task-header, topology,
  trit, LUT, recurrent-state, score, threshold, and mutation domain types.
- Added explicit little-endian 96-byte header serialization. The parser checks
  magic/version, dimensions, checked topology/data lengths, exact file length,
  role/index uniqueness and bounds, packed trit values `<243`, and digest
  metadata. Trailing bytes and truncation fail closed.
- Added canonical five-trit packing/unpacking; valid trits are exactly 0, 1,
  and 2, with 2 meaning UNKNOWN.
- Added dense logical LUT rows with 32-byte storage stride and a scalar,
  double-buffered recurrent tick. Every non-input update reads the previous
  state buffer and commits to the next buffer.
- Added one-window and full-window score paths, exact failure counting,
  timeout propagation, completion-aware score predicates, and runtime
  threshold predicates.
- Added canonical BPP9000 nonce checks (`nonce[0]==1`, `1<=nonce[1]<=10`,
  `nonce[2]==0`) and rejection of an all-zero mining seed.
- Added mutation selection, old/new value records, accept-if-`r <= current`,
  reverse-order rollback, and the default 100-step/101-score-call search.
- Added a seed-aware `CandidateRandomSource` boundary. Draw order and the
  64-byte random2-compatible padding sizes are explicit. The M1 fixture source
  is deterministic and non-cryptographic; no K12/random2 or signing code was
  copied or implemented.
- Added 100 generated small full-search cases and 10 independently generated
  production-shaped 44,744-byte cases. The production-shaped cases parse and
  execute one complete 672-sample window; they do not claim ten full
  production-score runs.
- Added `scripts/generate_corpus.sh` and a committed generator summary.

## Completed M2 work

- Added `src/xdna/` with typed errors, capability discovery, an explicit smoke
  buffer contract, XRT device/hardware-context setup, instruction loading,
  persistent buffers, dispatch/wait handling, and CPU-oracle evidence.
- Added `xdna_probe`, `xdna_smoke`, and pure `xdna_contract_tests` targets. The
  capability surface includes `SUPPORTED_XDNA1`, `NO_XDNA_DEVICE`,
  `WRONG_XDNA_GENERATION`, `XRT_UNAVAILABLE`, `DRIVER_UNAVAILABLE`,
  `FIRMWARE_UNAVAILABLE_OR_UNKNOWN`, `TOOLCHAIN_UNAVAILABLE`,
  `RUNTIME_VERSION_MISMATCH`, and `DEVICE_OPEN_FAILED`.
- Added the standalone Iron/MLIR-AIE smoke program and reproducible artifact
  builder. The device program contains no BPP9000 operation.
- Added project-owned `runtime-pins.json`, capability/artifact/smoke scripts,
  and `docs/evidence/m2-xdna-smoke.json`.
- The host path explicitly allocates instruction/input/output BOs, performs
  H2D and D2H synchronization, waits for `ERT_CMD_STATE_COMPLETED`, and has
  no CPU fallback. Context-creation failures have a distinct typed error.

The verified current host is Fedora Linux 45 prerelease on AMD Ryzen 7 250
with Radeon 780M. It reports `RyzenAI-npu1`, `aie2`, BDF `0000:06:00.1`,
`/dev/accel/accel0`, XRT topology `6x5`, and four available columns. The
recorded runtime pins are amdxdna/kernel
`7.2.0-0.rc5.260731.8ba098e6.443.vanilla.fc45.x86_64`, firmware `1.5.5.391`,
XRT `2.26.0` hash
`8bf2fc4c090540dcf7872243ab67779ae74ef5e3`, MLIR-AIE commit
`57d7494e99c214f5f53b328a0ed43a99e759e835`, `mlir_aie` `1.3.4`, CPython
`3.12.13`, and Peano `llvm-aie 21.0.0.2026072001+ce8c0f8f`. The artifact
uses one column, kernel `MLIR_AIE`, UUID
`de2ea958-76e1-55f1-190e-e8726125817c`, and workload
`int32[32] out[i] = 3 * in[i] + 7`.

The 100-dispatch acceptance run completed 100 XRT dispatches and 100 exact
CPU matches with zero output mismatches and zero runtime failures. It recorded
200 explicit H2D and 100 explicit D2H synchronizations. The related
`hawkpoint-npu-llm` checkout was reference-only; its old `...441...` kernel
pin differs from the current host's `...443...` stack, which was not changed.

## Completed M3 work

- Extended the M1 reference with `RecurrentState::load_current` and the public
  `recurrent_tick` oracle. It retains the scalar double-buffered semantics:
  input roles are held, updated rows read the previous state, and the next
  state is committed simultaneously.
- Added the typed K1 host contract under `src/xdna/k1.hpp` and `k1.cpp`:
  exact logical lengths, trit/topology validation, 32-byte LUT stride,
  deterministic pack/unpack, padding isolation, and exact mismatch indices.
- Added the clean-room AIE2 kernel in `src/xdna/k1_kernel.cc`. It performs
  only one physical recurrent LUT tick and never computes a CPU expected
  result or silently falls back to CPU.
- Added the one-column Iron/MLIR-AIE artifact generator and
  `scripts/build-xdna-k1.sh`. The device input is one 2,528-byte aligned arena:
  state at offset 0, LUT at 96, neighbors at 1,568, and updated rows at 2,336;
  the output BO is 96 bytes with a 64-byte logical prefix. This layout was
  selected after the compiler rejected the independent-stream shape because of
  the one-column DMA channel budget.
- Added `xdna_k1_differential`, deterministic edge/fixed/random vectors,
  mismatch JSON capture, and `scripts/run-m3-validation.sh`.

## Completed M4 work

- Added the public `CandidateMaterial` seam in the M1 reference so CPU-owned
  candidate materialization is shared exactly by the M4 verifier without
  moving mutation or random-source authority to the device.
- Added `src/xdna/m4.hpp/.cpp` with the fixed 15,488-byte input/128-byte output
  contract, M3-compatible 64-byte state/46x32 LUT/64x3 topology/18 input-role
  schema, trit and role validation, explicit timeout sentinel transport, and
  exact packed-output validation.
- Added `src/xdna/m4_score.hpp/.cpp` and `verification.*`. Raw NPU results are
  separate from the verified result; every window is independently recomputed
  by M1, compared field-for-field, and rejected on any mismatch. CPU retains
  state reset, window reduction, random materialization, mutation,
  accept/rollback, and candidate authority.
- Added the clean-room one-column AIE2 artifact in `m4_kernel.cc` and
  `m4_program.py`, plus `scripts/build-xdna-m4.sh`. It supports isolated K1,
  repeated-tick, and one-window signal-paced score modes with no CPU fallback.
- Added `m4_contract_tests`, deterministic fixed/random vectors, physical
  `xdna_m4_differential`, mismatch JSON diagnostics, and
  `scripts/run-m4-validation.sh`. The final driver keeps M2/M3 evidence in
  temporary files and leaves the committed M2/M3 records unchanged.
- Added `docs/evidence/m4-full-score-differential.json` with final physical
  device, toolchain, artifact, dispatch, differential, verification, and
  negative-path evidence.

## Completed M5 work

- Added the fixed-width M5 contract under `src/xdna/m5.hpp` and `m5.cpp`.
  One item is one complete independent M4 `WindowScore` operation with
  candidate index, window index, explicit state/LUT/topology/input/target
  offsets, 15,488-byte input stride, 128-byte output stride, result ordering,
  status, and per-item error fields. Contract validation rejects malformed
  shapes, trits, roles, topology, stale/sentinel output, and wrong result
  magic.
- Added `src/xdna/m5_kernel.cc` and `m5_program.py`. The clean-room device
  kernel runs the M4 window semantics only; CPU mutation, accept/rollback,
  reduction, and canonical verification remain host-owned. Fixed artifact
  variants exist for batch sizes 1, 2, 4, 8, and 16 with one, two, or four
  columns where divisible.
- Added explicit lane workers and corrected the runtime DMA tap to transfer
  every `items_per_lane` record. Generated `npu1_1col`, `npu1_2col`, and
  `npu1_4col`/partition metadata are retained under the build artifact paths
  and summarized in `docs/evidence/m5-batching-four-column.json`.
- Extended `XdnaRuntime` with M5 BO allocation/reuse, full input/output
  rewrites per dispatch, explicit H2D/D2H counters, dispatch-wait timing, and
  fail-closed per-item output validation. M4 and M5 measurements use separate
  hardware-context lifetimes because this host rejects concurrent contexts.
- Added `m5_contract_tests`, `xdna_m5_differential`,
  `scripts/build-xdna-m5.sh`, `scripts/aggregate-m5-evidence.py`, and
  `scripts/run-m5-validation.sh`. The runner exercises ordered, reversed,
  `A,A,B,A`, unique-lane, repeated-BO, mutation-visible, rollback, timeout,
  and finite-score cases against the M1 CPU oracle.

## Completed M6 work in this recovery checkpoint

- Revalidated the current public Qubic core/Qiner refs with `git ls-remote`;
  the pinned core `a83f935406cd006b5b1a94971139e74d410ecb6d` and Qiner
  `11fb18a6f4944bb55fe103d3f263cb5d31e00200` still match `main` and their
  recorded tags. No upstream source was copied.
- Added `src/qubic/direct_node.hpp/.cpp` with strict 8-byte frame parsing,
  incremental/partial-read handling, exact 128-byte `RespondSystemInfo`
  parsing, local task identity, BPP9000 algorithm selection, WorkContext
  freshness, CPU/NPU exact score/threshold/nonce gates, deterministic direct
  solution serialization, bounded TCP connect/read/write behavior, bounded
  reconnect, secret-safe environment configuration, and explicit live-send
  opt-in.
- Added an injected `CryptoProvider` boundary and fail-closed
  `UnavailableCryptoProvider`, plus the optional pinned
  `K12FourQCryptoProvider`. FourQlib v3.1 is pinned to
  `1031567f23278e1135b35cc04e5d74c2ac88c029` under MIT; selected K12 sources
  are pinned to `f95b0b73e29fe75fe99fbbb24c8000d9fcf0b40e` under the recorded
  CC0/public-domain and endian notices. The provider treats the secret as a
  32-byte signing subseed, derives/checks the public key, and supplies K12,
  compressed FourQ ECDH, and K12-backed SchnorrQ operations.
- Added `qubic_crypto_tests` with RFC 9861 K12 vectors, the synthetic Qubic
  SchnorrQ vector, exact synthetic public-key/shared-key/gamming-key/gamma
  vectors, and malformed/tamper rejection. The opt-in CTest gate passed 7/7;
  the default direct-node test provider remains deterministic and
  non-cryptographic only.
- Added `qubic_direct_node_tests` covering fragmented frames, system-info
  fields, stale context/seed, unsupported algorithm, task mismatch,
  CPU/NPU mismatch, invalid nonce, timeout, threshold rejection, malformed
  context, deterministic solution bytes, bounded reconnect, mock request and
  mock submission, disabled live submission, and secret redaction.
- Added `docs/evidence/m6-direct-node.json` with the recovery, protocol,
  offline/mock, no-send, live-gate, crypto-KAT, and regression state. M6
  remains **IN PROGRESS** because authorized live submission is not exercised;
  the read-only system-info interoperability gate passes.

## Upstream cross-check result

The implementation was checked against the M0-derived facts from the pinned
core/Qiner revisions: exact header field order and sizes, base-3 packing,
topology role/index rules, three-neighbor LUT indexing, simultaneous
previous-state reads, signal-paced window scoring, timeout propagation,
canonical nonce fields, mutation selection/replacement, accept-if-`r <=
current`, and the 100-step/101-call lifecycle. The new provider also passes
independent K12 and synthetic Qubic/FourQ byte vectors. The public node
handshake and system-info response are live-verified; production task-byte
compatibility and authorized submission remain separate unexercised gates.

## Files changed in M1

- `CMakeLists.txt`
- `src/bpp9000/types.hpp`
- `src/bpp9000/task.hpp`
- `src/bpp9000/task.cpp`
- `src/bpp9000/random.hpp`
- `src/bpp9000/random.cpp`
- `src/bpp9000/reference.hpp`
- `src/bpp9000/reference.cpp`
- `tests/test_main.cpp`
- `scripts/generate_corpus.sh`
- `docs/TESTING.md`
- `docs/DECISIONS.md`
- `docs/ARCHITECTURE.md`
- `docs/PROJECT_SPEC.md`
- `docs/MILESTONES.md`
- `docs/AI_HANDOFF.md`

M2 also changed or added:

- `CMakeLists.txt`, `runtime-pins.json`, and `tests/xdna_contract_tests.cpp`;
- `src/xdna/` runtime, smoke host, and Iron artifact sources;
- `scripts/verify-xdna1.sh`, `scripts/build-xdna-smoke.sh`, and
  `scripts/run-xdna-smoke.sh`;
- `docs/evidence/m2-xdna-smoke.json` and the M2 updates to
  `docs/TESTING.md`, `docs/ARCHITECTURE.md`, `docs/DECISIONS.md`,
  `docs/BENCHMARKS.md`, and `docs/UPSTREAM.md`.

M3 also changed or added:

- `src/bpp9000/reference.hpp` and `src/bpp9000/reference.cpp` for the K1
  oracle boundary;
- `src/xdna/k1.hpp`, `src/xdna/k1.cpp`, `src/xdna/k1_kernel.cc`,
  `src/xdna/k1_program.py`, and the K1 runtime additions;
- `tests/k1_vectors.hpp`, `tests/k1_vectors.cpp`,
  `tests/k1_contract_tests.cpp`, and `tests/k1_differential.cpp`;
- `scripts/build-xdna-k1.sh`, `scripts/run-m3-validation.sh`, and
  `docs/evidence/m3-k1-differential.json`;
- the M3 updates to `docs/AI_HANDOFF.md`, `docs/MILESTONES.md`,
  `docs/ARCHITECTURE.md`, `docs/TESTING.md`, `docs/DECISIONS.md`, and
  `docs/BENCHMARKS.md`.

M4 also changed or added:

- `src/bpp9000/reference.hpp` and `src/bpp9000/reference.cpp` for the shared
  candidate-material seam;
- `src/xdna/m4.hpp`, `src/xdna/m4.cpp`, `src/xdna/m4_score.hpp`,
  `src/xdna/m4_score.cpp`, `src/xdna/verification.hpp`,
  `src/xdna/verification.cpp`, and the M4 additions to `runtime.*`;
- `src/xdna/m4_kernel.cc`, `src/xdna/m4_program.py`,
  `scripts/build-xdna-m4.sh`, and `scripts/run-m4-validation.sh`;
- `tests/m4_vectors.*`, `tests/m4_contract_tests.cpp`, and
  `tests/m4_differential.cpp`;
- `docs/evidence/m4-full-score-differential.json`, plus the M4 updates to
  `docs/MILESTONES.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md`,
  `docs/DECISIONS.md`, `docs/BENCHMARKS.md`, and this handoff.

M5 also changed or added:

- `src/xdna/m5.hpp`, `src/xdna/m5.cpp`, `src/xdna/m5_kernel.cc`,
  `src/xdna/m5_program.py`, and the M5 additions to `src/xdna/runtime.*`;
- `tests/m5_contract_tests.cpp`, `tests/m5_differential.cpp`, and the M5
  CMake/CTest targets;
- `scripts/build-xdna-m5.sh`, `scripts/aggregate-m5-evidence.py`, and
  `scripts/run-m5-validation.sh`;
- `docs/evidence/m5-batching-four-column.json`, plus the M5 updates to
  `docs/MILESTONES.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md`,
  `docs/DECISIONS.md`, `docs/BENCHMARKS.md`, and this handoff.

M6 recovery checkpoint also changed or added:

- `src/qubic/direct_node.hpp`, `src/qubic/direct_node.cpp`, and
  `tests/qubic_direct_node_tests.cpp`;
- `src/qubic/production_crypto.hpp`, `src/qubic/production_crypto.cpp`, and
  `tests/qubic_crypto_tests.cpp`;
- the `qubic_direct_node` library and CTest target in `CMakeLists.txt`;
- the opt-in FourQlib/K12 FetchContent build and `qubic_crypto_tests` target;
- `docs/evidence/m6-direct-node.json`;
- the M6 updates to `docs/UPSTREAM.md`, `docs/PROJECT_SPEC.md`,
  `docs/MILESTONES.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md`,
  `docs/DECISIONS.md`, and this handoff.

## Tests and exact results

Commands:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/bpp9000_tests
./scripts/generate_corpus.sh build
```

The direct test executable passes 8 groups and 361 assertions. The corpus
command reports:

```text
generator_version=m1-v1
generated_cases=100
production_shaped_cases=10
generated_digest=2979889feed3352b3c12831a301a357b6c9099f3de80b955f152c53bca2f8c03
production_digest=7c1da1028b9ecdbae54616654606185e62076ff7b69e209ecbf3d23f6a2fede1
```

The fixed test vector and corpus were executed twice with byte-identical
results. `git diff --check` passes.

M2 commands and results:

```bash
./scripts/verify-xdna1.sh
./scripts/build-xdna-smoke.sh
./scripts/run-xdna-smoke.sh --iterations 1
./scripts/run-xdna-smoke.sh --iterations 100
python3 -m json.tool docs/evidence/m2-xdna-smoke.json
```

The capability probe and artifact build pass. The one-dispatch smoke and the
100-dispatch run both report `NPU SMOKE PASS`; the evidence JSON validates.
The M2 completion SHA is recorded above.

M3 commands and exact results:

```bash
./scripts/run-m3-validation.sh
python3 -m json.tool docs/evidence/m3-k1-differential.json
git diff --check
```

The final differential run exercised 37 edge cases, 100 fixed cases, and
1,000 seeded random cases with generator `m3-k1-v1` and seed
`5562880460839399681`. It completed 1,139 physical K1 dispatches with 1,139
successful dispatches and exact logical matches, zero mismatches, zero runtime
failures, 2,278 H2D synchronizations, and 1,139 D2H synchronizations. The
machine-readable record is `docs/evidence/m3-k1-differential.json`.

M4 commands and exact results:

```bash
./scripts/run-m4-validation.sh
python3 -m json.tool docs/evidence/m4-full-score-differential.json
git diff --check
```

The final physical run used the pinned random seed
`5562880460839399681` and completed:

```text
repeated_tick_cases=1000
one_window_cases=100
multi_window_cases=1000
fixed_cases=100
random_cases=1000
full_score_cases=11
production_shaped_cases=1
candidate_cases=2
physical_dispatches=13460
successful_dispatches=13460
exact_score_runs=213
exact_comparisons=4313
candidate_score_calls=202
candidate_window_comparisons=1212
score_mismatches=0
runtime_failures=0
explicit_h2d_syncs=26920
explicit_d2h_syncs=13460
```

The differential evidence records 12,460 compared windows, 34 timeout
matches, 3,279 finite score matches, and zero mismatches. The 11 full-score
runs include ten generated small cases and one independent production-shaped
`T=8760/W=672` case with all 8,088 windows. Each of the two candidate paths
completed 101 score calls and matched the standalone CPU candidate's final
current/best LUT and score state. The M4 contract tests also cover malformed
trits/topology/sequences, invalid window bounds, timeout serialization, and
score mismatch rejection.

M5 commands and exact results:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./scripts/run-m5-validation.sh
python3 -m json.tool docs/evidence/m5-batching-four-column.json
git diff --check
```

`run-m5-validation.sh` reruns M1 corpus/digest checks, M2 smoke, M3
differential, and M4 full-score validation before building and running the
M5 matrix. It physically accepted these `(batch_size, columns)` variants:
`(1,1)`, `(2,1)`, `(4,1)`, `(2,2)`, `(4,2)`, `(8,2)`, `(4,4)`, `(8,4)`,
and `(16,4)`. Each configuration used 16 logical items, two warm-ups, five
measured repeats, and 80/80 exact measured item matches with zero mismatches
and zero runtime failures. The runner also recorded exact ordered/reversed/
`A,A,B,A` isolation and mutation/rollback visibility matches.

The M4 identical-work baseline was 80 physical dispatches, 160 H2D syncs,
80 D2H syncs, 1,246,800 H2D bytes, 10,240 D2H bytes, and median/p95 wall
time 2.987789/3.251935 ms. The best raw M5 record was batch 16/four columns:
five dispatches, 10 H2D syncs, five D2H syncs, 1,249,280 H2D bytes, 10,240
D2H bytes, and median/p95 wall time 1.277969/1.516637 ms. M5 H2D bytes are
slightly larger because its fixed input stride includes 31 explicit padding
bytes per item. The full raw timing samples, artifact SHA-256 values,
instruction hashes, UUIDs, generated placement, and buffer footprints are in
`docs/evidence/m5-batching-four-column.json`; no speedup, hashrate, power,
energy, profitability, or network claim is made.

M6 offline/mock commands and exact results:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 -m json.tool docs/evidence/m6-direct-node.json
git diff --check
```

After recovery and the M6 additions, the default build completed with no
compiler errors and CTest reported `6/6` tests passed. The new direct-node test uses
one-byte response reads, bounded three-attempt reconnect with two injected
failures, exact mock system-info request parsing, deterministic test-only
solution serialization, and zero sends for every invalid-case gate. The
separate opt-in crypto build completed with no compiler errors and CTest
reported `7/7` tests passed, including RFC 9861 K12, Qubic SchnorrQ,
public-key, shared-key, gamming-key, gamma-stream, and malformed-input KATs.
Neither build claims live node interoperability.

Production crypto gate commands and result:

```bash
cmake -S . -B build-crypto -DCMAKE_BUILD_TYPE=Debug \
  -DXDNA_ENABLE_PRODUCTION_CRYPTO=ON
cmake --build build-crypto -j2
ctest --test-dir build-crypto --output-on-failure
```

`build-crypto` CTest result: `100% tests passed, 0 tests failed out of 7`.

The inherited post-recovery validation suite also passed:

- `./scripts/generate_corpus.sh build`: 100 generated and 10 production-shaped
  cases; generated digest `2979889feed3352b3c12831a301a357b6c9099f3de80b955f152c53bca2f8c03`;
  production digest `7c1da1028b9ecdbae54616654606185e62076ff7b69e209ecbf3d23f6a2fede1`.
- `./scripts/run-xdna-smoke.sh --iterations 100`: 100/100 exact matches,
  zero mismatches/runtime failures.
- `./scripts/run-m3-validation.sh`: 1,139/1,139 physical K1 matches,
  zero mismatches/runtime failures.
- `./scripts/run-m4-validation.sh`: 13,460/13,460 dispatches complete,
  4,313 exact comparisons, zero mismatches/runtime failures.
- `./scripts/run-m5-validation.sh`: all nine `(batch, columns)` variants
  accepted, 80/80 exact measured items each, zero mismatches/runtime failures;
  selected `(16,4)` median wall time `1.277969 ms` in this run. This is a raw
  M5 measurement, not a mining-rate or profitability claim.

## Hardware tests actually executed

The physical XDNA1/AIE2 path was exercised on the current host. `xrt-smi`
reported `RyzenAI-npu1`, firmware `1.5.5.391`, XRT `2.26.0`, and the current
amdxdna/kernel string recorded in `runtime-pins.json`. The generated K1
artifact was loaded into an XRT hardware context and dispatched 1,139 times
with exact output comparison against M1. The final artifact used one AIE2
column, kernel `MLIR_AIE`, xclbin UUID
`536772df-11a6-5d56-c865-0530d5ab17b1`, and the hashes recorded in the
evidence JSON.

The final M4 artifact was also loaded into an XRT hardware context on the same
device and dispatched 13,460 times. It used one AIE2 column, kernel
`MLIR_AIE`, xclbin SHA-256
`83d105882ea69d713bb7102cf093bf3aa5d356d76914168fcc50e812371ab7f9`,
instruction SHA-256
`cc811e1751208451a5979e117c91dc238809403602c58b098d1acd55edc3a5d6`, and
runtime UUID `2c63e13d-c176-422f-5b74-2069cac8e1d3`. M4 state was reset by the
host per window and held device-local only within a dispatch.

M5 physically loaded and ran all nine accepted fixed artifacts. The selected
batch-16/four-column artifact used xclbin SHA-256
`b6ea34031ddd74e36f71070856d3bbeee81421c5d96eda396660f00218989912`,
instruction SHA-256
`8d4aae92b9edde7b9e2c6725ba74b3dbd91bc5d1e2e800ab1c4b1667f9a861f5`, and
runtime UUID `388b19e1-b865-2818-b673-cb4727cef277`. Its generated partition
metadata reports width 4, start column 0, four row-2 workers, and lane item
ranges 0–3, 4–7, 8–11, and 12–15. Every lane processed distinct fixture
inputs and returned an exact CPU-verified result. The M4 baseline and M5
contexts were created in separate lifetimes because concurrent context setup
returns `DRM_IOCTL_AMDXDNA_CREATE_HWCTX` `err=-19` on this host.

The public Qubic node system-info exchange was exercised read-only twice. No
solution submission was attempted. M6 protocol behavior was exercised through
the offline/mock boundary, the live system-info probe, and the opt-in provider
KATs recorded in `docs/evidence/m6-direct-node.json`.

## Known limitations and unresolved behavior

1. The production task's topology/data hashes and random2-compatible candidate
   draws are KangarooTwelve-derived. The optional provider now supplies and
   tests K12 primitives, but production task loading/draw orchestration is not
   wired to live work acquisition and must not be inferred from fixtures.
2. The canonical production task bytes were not copied into this repository;
   M0 recorded their expected hashes. M1 production-shaped fixtures are
   independently generated and are not network truth.
3. Qatum/pool wire behavior remains unresolved and deferred. M1 does not
   depend on Qatum, QLI, or any pool.
4. M5 records raw timing, transfer, and dispatch measurements for an identical
   16-item comparison workload. They are not converted into a speedup,
   hashrate, energy, or profitability claim; M3/M4 records remain correctness
   evidence only.
5. Optional ASAN/UBSAN builds were attempted but the development image lacks
   the linker runtimes (`libasan.so.8.0.0` and `libubsan.so.1.0.0`). The normal
   warning-clean build and complete test suite pass.
6. A second-generation device is not present, so a physical
   `WRONG_XDNA_GENERATION` run was not available. Forced device-execution,
   context-creation, and output-mismatch failures were not manufactured on the
   healthy device; their typed fail-closed paths are implemented.
7. M5 does not retain a device-resident task/LUT/context across logical
   mutations or task changes. It safely reuses XRT BO allocations by rewriting
   the full input/output arenas every dispatch. The selected batch-16/four-
   column configuration is a local compute backend only. M6 wraps it with an
   offline-tested direct-node boundary and an opt-in, KAT-tested production
   crypto provider; live interoperability remains unavailable.

## Architectural decisions to preserve

- CPU owns task validation, random/K12 orchestration, mutation control,
  accept/rollback, threshold/freshness policy, and canonical verification.
- Exact integer equality is mandatory; no tolerance, saturation, signed-trit
  reinterpretation, or silent CPU fallback is allowed.
- The value `2` is UNKNOWN, never `-1`.
- The first future XDNA mapping is independent candidate/window work across
  complete lanes; do not split a recurrent candidate across columns before
  measuring synchronization cost.
- M2's smoke artifact is one-column `int32[32]` arithmetic only. It proves the
  runtime boundary and is not a BPP9000 kernel, mining benchmark, or four-column
  utilization result.
- M3's K1 artifact is one-column isolated recurrent compute only. Its exact
  logical contract and combined device arena are recorded in the M3 evidence;
  do not treat its dispatch count as a performance result.
- M4's artifact is one-column repeated-tick/one-window scoring. The host
  resets each window to UNKNOWN, transfers the full operation arena, compares
  every result to the M1 scalar oracle, and performs full-score reduction;
  `persistent_buffers=false` is intentional. CPU remains the only candidate
  and submission authority.
- M5's selected backend is a fixed batch-16/four-column artifact for complete
  independent window operations. Input stride is 15,488 bytes, output stride
  is 128 bytes, lane order is contiguous and stable, BO allocations are
  persistent but full arenas are rewritten/sentinel-initialized per dispatch,
  and CPU recomputation remains mandatory. Do not silently change the work
  unit to candidate mutation/search or split one recurrent window across
  columns.
- Direct-node integration is the current M6 protocol path. Qatum is optional
  and must wait for a stable authoritative wire specification; M7's continuous
  supervisor remains out of scope.

## Things the next agent MUST NOT redo

- Do not recreate the repository or repeat M0 research without a concrete
  contradiction.
- Do not copy Anti-Military-licensed Qubic source, Qiner source, QLI source,
  crypto code, or upstream task bytes.
- Do not replace the fixture random/digest seams with an unreviewed crypto
  implementation while calling M1 complete.
- Do not add AVX/SIMD, GPU, Qatum, pool, or continuous mining-loop code while
  extending the verified M5 backend. M6 network code must remain inside the
  finite direct-node boundary and behind the CPU/crypto/live-send gates.
- Do not claim the production-shaped corpus is the canonical task or a
  production performance benchmark.
- Do not claim speedup, power, energy, profitability, or mining hashrate from
  any M2/M3/M4 correctness record or from the M5 raw timings.
- Do not redo the M3 K1 differential run unless source, toolchain, artifact, or
  contract changes require it.
- Do not redo the M4 physical run unless the M4 source, toolchain, artifact, or
  contract changes require it. Do not silently convert the one-column M4
  baseline into a batch or four-column experiment; use the checked-in M5
  runner/evidence for that comparison.

## Exact next task: finish the M6 gate or checkpoint it

Do not start M7. The inherited physical M1–M5 validation suite and the
production-crypto KAT gate have passed. If an explicitly authorized source
identity, current computor destination, current task-compatible work, safe
signing material, and a CPU/NPU-verified threshold-eligible candidate become
available, exercise only the bounded live submission gate with explicit
opt-in. Otherwise preserve
`LIVE_SUBMISSION_NOT_EXERCISED_PROTOCOL_REQUIRES_AUTHORIZED_IDENTITY`, keep M6
**IN PROGRESS**, and record the external blocker. Do not weaken CPU
verification, infer task bytes, or claim profitability.

## Resume commands

```bash
cd /home/umutcagand/xdna-npu-miner
git status -sb
git branch --show-current
git log --oneline -15
git rev-parse HEAD
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
cmake -S . -B build-crypto -DCMAKE_BUILD_TYPE=Debug -DXDNA_ENABLE_PRODUCTION_CRYPTO=ON
cmake --build build-crypto -j2
ctest --test-dir build-crypto --output-on-failure
./scripts/generate_corpus.sh build
./scripts/run-m3-validation.sh
./scripts/run-m4-validation.sh
./scripts/run-m5-validation.sh
python3 -m json.tool docs/evidence/m5-batching-four-column.json
```

# Project Specification

## M0 status and scope

This repository is a standalone research miner for AMD Ryzen AI Hawk Point:
XDNA1, AIE2, four columns, `RyzenAI-npu1`, with Fedora Linux as the primary
target. No mining implementation exists in M0.

M0 is **COMPLETE**. The current direct-node algorithm, work lifecycle, score
semantics, validation behavior, and wire path are pinned below. Official Qubic
sources disagree about Qatum's status: the current FAQ describes it as a
stratum-like protocol “currently in development,” while an older official
Qubic blog post says it launched and the current ecosystem entry labels it
live. The conflict is recorded in `docs/UPSTREAM.md`; it does not provide a
sufficiently complete, authoritative wire specification for this project to
implement or claim compatibility.

The direct Qubic-node path is the canonical protocol path for this project and
is sufficient to close M0. M1 through M5 have **zero dependency** on Qatum or
any mining pool. M6 must implement and validate direct-node integration first.
Qatum/pool integration is optional after that path works and must wait until a
versioned, authoritative, sufficiently complete wire specification or
implementation can be pinned and independently reviewed. Pool-specific
protocols belong in adapters; they are not part of the mining/scoring core.

## Current milestone

**M1 — CPU golden reference — NOT STARTED**

## Current algorithm contract

The current active algorithm in Qubic core is **BPP9000**, selected by
`nonce[0] == 1`. `Neuraxon` is reserved in the current source and is not an
active implementation. The pinned production parameters in Qubic core
`v1.301.3` (`a83f935406cd006b5b1a94971139e74d410ecb6d`) are:

| Parameter | Value | Meaning |
|---|---:|---|
| `N` | 18 | input neurons/trits per sample |
| `M` | 1 | output neuron/trit per sample |
| `T` | 8,760 | sequence samples (`24 * 365`) |
| `W` | 672 | training window (`24 * 28`) |
| `P` | 64 | neurons |
| `K` | 3 | neighbors per non-input neuron |
| `S` | 100 | mutation steps |
| `L` | 1..10 | mutations per step, encoded in `nonce[1]` |
| `U` | 46 | updated neurons (`P - N`) |
| windows | 8,088 | `T - W` |
| max ticks | 100,000 | per training window |
| default threshold | 3,838 | lower is better; good if `score <= threshold` |

The scorer is a recurrent ternary ANN. Every neuron state and LUT entry is an
unsigned byte representing trit `0`, `1`, or `2`; `2` means **UNKNOWN**, not a
negative number. At each tick, each non-input neuron reads the three neighbor
states from the previous state buffer, computes `t0 + 3*t1 + 9*t2`, and reads
one byte from its 27-entry LUT. All next states commit simultaneously. Input
neurons are fed from the task while the signal remains unknown, then are set
to unknown after the training window has been consumed.

For each of 8,088 windows, the reference path resets all 64 states to unknown,
feeds up to 672 rows, advances ticks until the signal settles or 100,000 ticks
are reached, and compares the output with the target row at the end of the
window. The score is the `uint32_t` count of failed output comparisons. A
timeout returns `0xffffffff` for the candidate. One candidate performs one
initial score plus 100 mutation-attempt scores: **101 score calls**. A mutation
selects one of `U * 27 = 1,242` logical LUT entries and changes its trit to one
of the other two values. The mutation is accepted when the new score is no
worse (`r <= current`); the current implementation has exploration disabled
(`K == 0`).

The exact scorer and task format are documented in the pinned source records
in `docs/UPSTREAM.md`. M1 must independently reimplement these semantics in a
readable scalar CPU reference; it must not copy the upstream implementation.

## Work context and lifecycle

For the direct-node design, “work” is not a per-job ANN tensor sent in a job
message. It is the combination of:

1. the locally installed, hash-verified `bpp9000.task` file;
2. the computor public key used for the miner's candidate context;
3. the current 32-byte epoch mining seed;
4. the current solution threshold from system information; and
5. a candidate nonce satisfying the BPP9000 encoding.

The task topology/data and its hashes are constant while the node accepts that
task. The random2 pool is generated once from the epoch-start spectrum digest
and is kept fixed for the epoch according to the core source comment. The
mining seed changes at an epoch transition; a candidate using a zero or stale
seed is rejected by the current scorer. The nonce changes for each candidate;
`nonce[1]` controls the mutation count per step and bytes 3..31 influence the
candidate mutation stream. The threshold is runtime state and must not be
hard-coded from a reference miner.

At the direct-node boundary, a miner obtains the current epoch/tick/seed and
threshold through the system-info response, computes candidates locally,
recomputes any claimed result canonically on the CPU, and only then submits a
signed solution broadcast. The node verifies the seed, nonce, score and
threshold, deduplicates accepted broadcasts, and later packages an accepted
solution into a mining-solution transaction. A pool may wrap this lifecycle,
but no stable official pool message schema is currently available.

## Fixed task representation

The production task file is 44,744 bytes:

```text
96-byte packed little-endian header
848-byte topology block
43,800-byte sample data block
```

The header contains magic `0x5454554c`, version `1`, dimensions, and 32-byte
topology/data hashes. The topology contains input indices, output indices,
signal index, and `P*K` neighbor indices. Each sample packs five base-3 trits
per byte (`t0 + 3*t1 + 9*t2 + 27*t3 + 81*t4`); four bytes encode the 18 inputs
and one byte encodes the one output. Invalid packed values `>= 243` must be
rejected. The canonical production hashes are:

```text
topology: 13e99d5b2fca56aa789cb959575f48392f1a44909a8eaf27f2de8f8d74b07a6b
data:     979cdc2247d2ca4ed3d614bf27896384cb1c9c3d804af6ede6b59fc52c0e3dfa
```

These are from `qubic/core` `v1.301.3`; the Qiner example task has the same
dimensions and file size but different hashes/data and is not the production
network task.

## Responsibility boundary

```text
Qubic node / (future versioned pool adapter)
                    |
                    v
CPU network, epoch/job state, task validation, reconnect
                    |
                    v
CPU candidate generation, mutation control, nonce policy
                    |
                    v
batched XDNA1 recurrent scoring primitive(s)
                    |
                    v
CPU canonical score verification and threshold check
                    |
                    v
CPU signed direct-node submission / future pool share adapter
```

The CPU owns protocol parsing, identity/signing, seed/threshold freshness,
candidate acceptance/rollback, and the final canonical verification gate. The
NPU is a compute backend only. Candidate-level batching is the first XDNA1
mapping to investigate because independent candidates can each retain complete
recurrent state without cross-column tick synchronization. No performance
claim is made in M0.

## Initial success definition

A later release may call itself a functioning experimental miner only after it
can, on the documented target stack, load and hash-check the canonical task,
track a current seed/threshold, produce the exact CPU reference score, execute
the selected path on XDNA1 with dispatch evidence, pass the CPU/NPU contract,
recompute before submission, and demonstrate version-compatible authorized
node interoperability. Pool support is a separate gate because its current
protocol is not stable.

## Non-goals

- No mining, network, NPU kernel, or M1 implementation in M0.
- No assumption that an ANN-like workload automatically benefits from an NPU.
- No AVX optimization in the M1 oracle.
- No silent CPU fallback reported as NPU execution.
- No profitability, speedup, or energy claim without measured evidence.
- No coupling to `hawkpoint-npu-llm`; that repository is a reference for a
  validated XDNA1 environment only.
- No reuse of Qubic source code until the license audit permits it.

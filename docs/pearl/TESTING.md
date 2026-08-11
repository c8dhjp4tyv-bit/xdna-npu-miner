# Pearl Testing

Run CPU, gateway/work contracts, full CTest, physical P2/P3/P5 differentials,
P8/P9 benchmarks, CLI modes, the P10 endurance harness, and the official P7
SIMNET gate in that order. Every XDNA record includes exact CPU parity,
dispatch completion, transfer counters, and `cpu_fallbacks: 0`. Gateway tests
use bounded local Unix sockets and cover malformed JSON, invalid base64,
oversized targets, unsafe endpoints, and rejection categories.

## P7 official SIMNET gate

The official checkout is external to this repository and must be pinned to
`fe22b6a2b831d95b2f56564808f39d2f498f34a5`. Build `pearld`, run it on a
loopback SIMNET RPC port, and run the official gateway/prover in an isolated
CPU-only Python environment. Do not install or launch the official
CUDA/vLLM miner. The exact successful run used `/tmp/pearl-xdna-p7-official`,
`127.0.0.1:44107`, and `/tmp/pearlgw.sock`; credentials were transient and
must not enter logs committed to the repository.

The required proof sequence is:

```text
official getMiningInfo -> physical XDNA search -> project CPU verification
-> official bincode PlainProof -> official submitPlainProof
-> official gateway submitblock -> pearld accepted block
```

The recorded run found the proof on attempt 21, submitted a 140225-byte
official wire payload, and advanced SIMNET from height 1 to height 2 with
zero CPU fallbacks. The control CPU proof is recorded separately and is not
counted as XDNA evidence. The machine-readable result is
`docs/evidence/pearl-p7-e2e.json`; no full endurance rerun is required because
the P7 source change is a protocol adapter/boundary fix, not an endurance
algorithm change.

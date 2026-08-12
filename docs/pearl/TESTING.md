# Pearl Testing

## Certificate V3 upgrade verification (2026-08-12)

The V3 upgrade is an additive test record; the historical V2/P7 records below
remain unchanged. Run the CPU oracle first, then the physical differential,
then the current official SIMNET gate. Do not attempt mainnet preparation
until the SIMNET V3 block is accepted.

```bash
cmake -S . -B build
cmake --build build --target pearl_cpu_tests pearl_gateway_tests pearl_work_tests \
  pearl_v3_xdna_differential pearl-xdna-miner -j2
./build/pearl_cpu_tests
./build/pearl_gateway_tests
./build/pearl_work_tests
./build/pearl-xdna-miner --self-test
./build/pearl_v3_xdna_differential \
  --xclbin build/pearl-xdna-gemm-p2-c4/pearl_p2_gemm.xclbin \
  --insts build/pearl-xdna-gemm-p2-c4/pearl_p2_gemm.insts \
  --manifest build/pearl-xdna-gemm-p2-c4/pearl_p2_gemm.manifest \
  --selector 0 --deterministic-cases 100 --randomized-cases 32 \
  --evidence docs/evidence/pearl-v3-xdna-differential.json
```

The CPU corpus verifies independently derived V3 domain keys, 64-byte
root/dimension bindings, V3 seeds, proof-commitment version domain, official
raw-root behavior, and all rejection cases. V2 vectors remain byte-identical.
The physical record requires 100 deterministic and 32 randomized V3 cases
with zero arithmetic, seed, transcript, or jackpot mismatches and zero CPU
fallbacks. A five-minute physical V3 stability record is separate from the
historical P10 endurance evidence because the AIE2 kernel itself did not
change.

For the current official runtime, build external Pearl `bfd0647` / 1.4.2 and
use CPU-only `py-pearl-mining 0.3.0` plus `pearl-gateway 0.1.0`; do not launch
the CUDA/vLLM miner. The required current SIMNET path is:

```text
official getMiningInfo (cert_version 3)
-> V3 salted seeds -> physical XDNA -> CPU verification
-> official PlainProof verifier -> official gateway -> pearld accepted block
```

For mainnet, bind the node RPC to loopback, use transient strong local
credentials, and run no submission. The miner must be explicitly invoked as
`--dry-run --network mainnet`; it reports `MAINNET_DRY_RUN_PASS` only after a
real gateway job was parsed, a physical candidate was verified, and its fresh
identity check passed. With no configured public mainnet payout address, do
not start the gateway or fabricate an address; record
`MAINNET_PAYOUT_ADDRESS_NOT_CONFIGURED` and continue node-only sync checks.

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

# Pearl XDNA Miner Operations

The delivered binary is `pearl-xdna-miner`. Live mining is opt-in: invoking
it without a mode only prints help, and `--mine` requires an explicit public
`--mining-address`. No seed, private key, password, or auth token is accepted
by the CLI output path.

## Build and artifacts

Required on the primary target are a C++20 compiler, CMake, Cargo, XRT,
`amdxdna`, and the MLIR-AIE Iron environment at `${HOME}/mlir-aie/ironenv`.
Build the CPU/XRT targets with:

```bash
cargo build --manifest-path src/pearl/blake3_ffi/Cargo.toml --release --locked
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./scripts/build-pearl-xdna-gemm.sh build 4
```

The artifact builder writes `build/pearl-xdna-gemm-p2-c4/` with the xclbin,
instruction blob, manifest, layout JSON, and SHA-256 records. The artifact is
not a CPU fallback: the host output BO is poisoned before every XRT dispatch.

## Safe checks

```bash
./build/pearl-xdna-miner --help
./build/pearl-xdna-miner --version
./build/pearl-xdna-miner --hardware-info --json-status
./build/pearl-xdna-miner --self-test --artifact-dir build/pearl-xdna-gemm-p2-c4
./build/pearl-xdna-miner --benchmark --artifact-dir build/pearl-xdna-gemm-p2-c4
```

`--self-test` reports `CPU_SELF_TEST_PASS`, then either
`XDNA_SELF_TEST_PASS`, `XDNA_MISMATCH`, or `XDNA_DEVICE_UNAVAILABLE` and
returns nonzero for a physical failure. `--benchmark` is exact P2 GEMM work,
not a profitability or hashrate estimate.

## Gateway, dry-run, and mine

The pinned Pearl gateway is newline-delimited JSON-RPC, using either
`/tmp/pearlgw.sock` or loopback TCP port 8337. The client implements only
`getMiningInfo` and `submitPlainProof`; no Stratum method is invented.

```bash
./build/pearl-xdna-miner --dry-run --json-status
./build/pearl-xdna-miner --dry-run --fixture-work \
  --artifact-dir build/pearl-xdna-gemm-p2-c4 --json-status
./build/pearl-xdna-miner --mine \
  --mining-address PUBLIC_TAPROOT_ADDRESS \
  --artifact-dir build/pearl-xdna-gemm-p2-c4
```

The normal dry-run acquires current gateway work and stops before submission.
The fixture mode is deterministic local verification only; it never contacts
or submits to a gateway. The current official gateway's A/B tensors come from
its external useful-work/inference provider. That provider is intentionally an
explicit unavailable boundary in this repository, not a source for fabricated
live matrices.

Node RPC credentials, when a future node adapter is used, come from
`PEARL_NODE_RPC_USER` and `PEARL_NODE_RPC_PASSWORD`. They are never logged.
`config.example.toml` contains placeholders only.

## Status, shutdown, and recovery

Use `--json-status` for bounded machine-readable state. `--batch` and
`--columns` are configuration values, not claims that an artifact has been
compiled for that placement. Ctrl-C and SIGTERM are handled by the endurance
supervisor and stop dispatching cleanly. A device error, stale job, malformed
response, proof failure, or gateway rejection is categorized and causes the
candidate to be discarded; obsolete work is never submitted.

No systemd unit, autostart, persistence, remote deployment, wallet creation,
or hidden resource consumption is installed by this project.

## Current external limits

The physical Hawk Point XDNA1 path is verified. No official Pearl node or
gateway process is installed in the current workspace, so live gateway,
official prover, and local/simnet block acceptance remain externally blocked.
The P1 PlainProof is the repository-owned pre-prover envelope; the official
gateway/prover may own certificate/ZK generation where the pinned hot-component
license boundary is unresolved.

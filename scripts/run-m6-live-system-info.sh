#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${script_dir}/.." && pwd)
probe="${repo_dir}/build/qubic_live_probe"

if [[ ! -x "${probe}" ]]; then
    echo "missing ${probe}; build the Debug tree first" >&2
    exit 2
fi

# Official Qubic public direct-network endpoint documented by the Qubic Team:
# https://qubic.org/blog-detail/how-to-query-qubic-oracle-machines-using-the-qubic.net-toolkit
# The probe is read-only and never loads signing material or submits a frame.
export XDNA_QUBIC_NODE_HOST='corenet.qubic.li'
export XDNA_QUBIC_NODE_PORT='21841'
export XDNA_QUBIC_ALLOW_LIVE_SUBMISSION='0'
unset XDNA_QUBIC_SIGNING_PUBLIC_KEY_HEX
unset XDNA_QUBIC_SIGNING_SECRET_HEX

exec "${probe}" --repeat 2 --attempts 2 --timeout-ms 3000

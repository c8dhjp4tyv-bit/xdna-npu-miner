#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${script_dir}/.." && pwd)
build_dir=${XDNA_M6_BUILD_DIR:-build-crypto}
if [[ "${build_dir}" != /* ]]; then
    build_dir="${repo_dir}/${build_dir}"
fi
tool="${build_dir}/m6_authorization_check"
secret_dir=${XDNA_M6_SECRET_DIR:-${repo_dir}/.local-secrets}
secret_path=${XDNA_M6_SECRET_PATH:-${secret_dir}/m6-signing-subseed}
host=${XDNA_QUBIC_NODE_HOST:-corenet.qubic.li}
port=${XDNA_QUBIC_NODE_PORT:-21841}

if [[ ! -x "${tool}" ]]; then
    echo "missing ${tool}; configure with -DXDNA_ENABLE_PRODUCTION_CRYPTO=ON and build first" >&2
    exit 2
fi

exec "${tool}" --secret-path "${secret_path}" --host "${host}" --port "${port}"

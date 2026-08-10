#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${script_dir}/.." && pwd)
build_dir=${XDNA_M6_BUILD_DIR:-build-crypto}
if [[ "${build_dir}" != /* ]]; then
    build_dir="${repo_dir}/${build_dir}"
fi
tool="${build_dir}/m6_read_only_diagnostics"
host=${XDNA_QUBIC_NODE_HOST:-corenet.qubic.li}
port=${XDNA_QUBIC_NODE_PORT:-21841}
repeat=${XDNA_M6_DIAGNOSTIC_REPEAT:-3}
secret_path=${XDNA_M6_SECRET_PATH:-${repo_dir}/.local-secrets/m6-signing-subseed}

if [[ ! -x "${tool}" ]]; then
    echo "missing ${tool}; configure with -DXDNA_ENABLE_PRODUCTION_CRYPTO=ON and build first" >&2
    exit 2
fi

# This is an intentionally bounded, read-only matrix. Individual query
# failures are diagnostic observations; the executable still returns 2 for a
# configuration/tool failure and never sends a solution frame.
set +e
"${tool}" --sequence all --repeat "${repeat}" --host "${host}" --port "${port}" \
    --secret-path "${secret_path}"
status=$?
set -e
if [[ "${status}" -eq 2 ]]; then
    exit 2
fi
exit 0

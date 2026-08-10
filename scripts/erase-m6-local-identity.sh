#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${script_dir}/.." && pwd)
build_dir=${XDNA_M6_BUILD_DIR:-build-crypto}
if [[ "${build_dir}" != /* ]]; then
    build_dir="${repo_dir}/${build_dir}"
fi
tool="${build_dir}/m6_identity_tool"
secret_dir=${XDNA_M6_SECRET_DIR:-${repo_dir}/.local-secrets}
secret_path=${XDNA_M6_SECRET_PATH:-${secret_dir}/m6-signing-subseed}

if [[ ! -x "${tool}" ]]; then
    echo "missing ${tool}; configure with -DXDNA_ENABLE_PRODUCTION_CRYPTO=ON and build first" >&2
    exit 2
fi
if [[ "${secret_path}" == '/' || -z "${secret_path}" ]]; then
    echo 'refusing an empty or root local identity path' >&2
    exit 2
fi

exec "${tool}" erase --path "${secret_path}"

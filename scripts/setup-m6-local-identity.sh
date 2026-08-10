#!/usr/bin/env bash
set -euo pipefail
umask 077

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
if [[ -L "${secret_dir}" || ( -e "${secret_dir}" && ! -d "${secret_dir}" ) ]]; then
    echo 'local identity directory must not be a symlink or non-directory' >&2
    exit 2
fi
mkdir -p -- "${secret_dir}"
chmod 700 -- "${secret_dir}"
if [[ "$(stat -c '%a' -- "${secret_dir}")" != '700' ]]; then
    echo 'local identity directory could not be restricted to mode 0700' >&2
    exit 2
fi

replace=()
for argument in "$@"; do
    if [[ "${argument}" == '--replace' ]]; then
        replace+=(--replace)
    else
        echo "unknown option: ${argument}" >&2
        exit 64
    fi
done

exec "${tool}" generate --path "${secret_path}" "${replace[@]}"

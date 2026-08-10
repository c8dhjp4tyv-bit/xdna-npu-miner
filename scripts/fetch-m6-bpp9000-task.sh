#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${script_dir}/.." && pwd)
readonly task_url='https://raw.githubusercontent.com/qubic/core/a83f935406cd006b5b1a94971139e74d410ecb6d/data/bpp9000.task'
readonly expected_bytes='44744'
readonly expected_sha256='0c5e9e42c6d86c320af62f4125ca85b2446f2b098893fd6521bcf66c22f7f00a'
cache_path=${XDNA_M6_TASK_CACHE_PATH:-${repo_dir}/.local-cache/m6-bpp9000.task}
if [[ "${cache_path}" != /* ]]; then
    cache_path="${repo_dir}/${cache_path}"
fi
refresh=false
for argument in "$@"; do
    if [[ "${argument}" == '--refresh' ]]; then
        refresh=true
    else
        echo "unknown option: ${argument}" >&2
        exit 64
    fi
done

cache_dir=$(dirname -- "${cache_path}")
if [[ -L "${cache_dir}" || ( -e "${cache_dir}" && ! -d "${cache_dir}" ) ]]; then
    echo 'task cache directory must not be a symlink or non-directory' >&2
    exit 2
fi
mkdir -p -- "${cache_dir}"

verify_file() {
    local candidate=$1
    [[ -f "${candidate}" && ! -L "${candidate}" ]] || return 1
    local size
    size=$(stat -c '%s' -- "${candidate}")
    [[ "${size}" == "${expected_bytes}" ]] || return 1
    local digest
    digest=$(sha256sum -- "${candidate}" | awk '{print $1}')
    [[ "${digest}" == "${expected_sha256}" ]]
}

if [[ -e "${cache_path}" || -L "${cache_path}" ]]; then
    if verify_file "${cache_path}"; then
        echo "task_path=${cache_path}"
        echo "task_size_bytes=${expected_bytes}"
        echo "task_sha256=${expected_sha256}"
        echo 'task_cache=verified_existing'
        exit 0
    fi
    if [[ "${refresh}" != 'true' ]]; then
        echo 'existing task cache failed the pinned size or SHA-256 check; use --refresh explicitly' >&2
        exit 2
    fi
fi

temporary_dir=$(mktemp -d "${TMPDIR:-/tmp}/xdna-m6-task.XXXXXX")
cleanup() {
    rm -rf -- "${temporary_dir}"
}
trap cleanup EXIT
temporary_path="${temporary_dir}/bpp9000.task"
curl --fail --silent --show-error --location --proto '=https' --tlsv1.2 \
    --connect-timeout 10 --max-time 120 --retry 2 --retry-delay 1 \
    --output "${temporary_path}" "${task_url}"
if ! verify_file "${temporary_path}"; then
    echo 'downloaded task failed the pinned size or SHA-256 check' >&2
    exit 2
fi
mv -- "${temporary_path}" "${cache_path}"
chmod 600 -- "${cache_path}"
echo "task_path=${cache_path}"
echo "task_size_bytes=${expected_bytes}"
echo "task_sha256=${expected_sha256}"
echo 'task_cache=refreshed_from_pinned_core_revision'

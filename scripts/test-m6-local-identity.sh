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
if [[ ! -x "${tool}" ]]; then
    echo "missing ${tool}; build the production crypto tree first" >&2
    exit 2
fi
git -C "${repo_dir}" check-ignore -q .local-secrets/m6-signing-subseed

temporary_dir=$(mktemp -d "${TMPDIR:-/tmp}/xdna-m6-identity-test.XXXXXX")
cleanup() {
    rm -rf -- "${temporary_dir}"
}
trap cleanup EXIT
secret_dir="${temporary_dir}/secrets"
secret_path="${secret_dir}/m6-signing-subseed"
mkdir -- "${secret_dir}"
chmod 700 -- "${secret_dir}"

safe_output_check() {
    local output_file=$1
    local secret_value=$2
    ! grep -F -q -- "${secret_value}" "${output_file}"
}

"${tool}" generate --path "${secret_path}" \
    >"${temporary_dir}/generate.out" 2>"${temporary_dir}/generate.err"
[[ "$(stat -c '%a' -- "${secret_dir}")" == '700' ]]
[[ "$(stat -c '%a' -- "${secret_path}")" == '600' ]]
secret_hex=$(tr -d '\n' <"${secret_path}")
[[ "${#secret_hex}" == '64' ]]
[[ "${secret_hex}" != *[^0-9a-fA-F]* ]]
safe_output_check "${temporary_dir}/generate.out" "${secret_hex}"
safe_output_check "${temporary_dir}/generate.err" "${secret_hex}"
grep -q '^identity=[A-Z]\{60\}$' "${temporary_dir}/generate.out"
grep -q '^public_key_hex=[0-9a-f]\{64\}$' "${temporary_dir}/generate.out"
grep -q '^secret_output=never$' "${temporary_dir}/generate.out"

"${tool}" show --path "${secret_path}" \
    >"${temporary_dir}/show.out" 2>"${temporary_dir}/show.err"
safe_output_check "${temporary_dir}/show.out" "${secret_hex}"
safe_output_check "${temporary_dir}/show.err" "${secret_hex}"
grep -q '^identity=' "${temporary_dir}/show.out"
before_digest=$(sha256sum -- "${secret_path}" | awk '{print $1}')
if "${tool}" generate --path "${secret_path}" \
    >"${temporary_dir}/overwrite.out" 2>"${temporary_dir}/overwrite.err"; then
    echo 'identity overwrite unexpectedly succeeded without --replace' >&2
    exit 1
fi
after_digest=$(sha256sum -- "${secret_path}" | awk '{print $1}')
[[ "${before_digest}" == "${after_digest}" ]]
safe_output_check "${temporary_dir}/overwrite.out" "${secret_hex}"
safe_output_check "${temporary_dir}/overwrite.err" "${secret_hex}"

if "${tool}" show --path "${temporary_dir}/missing" \
    >"${temporary_dir}/missing.out" 2>"${temporary_dir}/missing.err"; then
    echo 'missing identity unexpectedly passed closed-file check' >&2
    exit 1
fi
safe_output_check "${temporary_dir}/missing.out" "${secret_hex}"
safe_output_check "${temporary_dir}/missing.err" "${secret_hex}"

git -C "${repo_dir}" diff -- >"${temporary_dir}/git-diff.out"
git -C "${repo_dir}" diff --no-index -- /dev/null "${repo_dir}/docs/evidence/m6-direct-node.json" \
    >"${temporary_dir}/git-no-index.out" 2>&1 || true
for output_file in "${temporary_dir}"/*.out "${temporary_dir}"/*.err; do
    safe_output_check "${output_file}" "${secret_hex}"
done

"${tool}" erase --path "${secret_path}" \
    >"${temporary_dir}/erase.out" 2>"${temporary_dir}/erase.err"
[[ ! -e "${secret_path}" ]]
safe_output_check "${temporary_dir}/erase.out" "${secret_hex}"
safe_output_check "${temporary_dir}/erase.err" "${secret_hex}"
echo 'PASS m6_local_identity_safety'

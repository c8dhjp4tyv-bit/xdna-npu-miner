#!/usr/bin/env bash
set -euo pipefail

if [[ "${XDNA_QUBIC_ALLOW_LIVE_SUBMISSION:-0}" != '1' ]]; then
    echo 'LIVE_SUBMISSION_DISABLED: set XDNA_QUBIC_ALLOW_LIVE_SUBMISSION=1 for an explicit opt-in' >&2
    exit 2
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${script_dir}/.." && pwd)
build_dir=${XDNA_M6_BUILD_DIR:-build-crypto}
if [[ "${build_dir}" != /* ]]; then
    build_dir="${repo_dir}/${build_dir}"
fi
identity_tool="${build_dir}/m6_identity_tool"
authorization_tool="${build_dir}/m6_authorization_check"
task_tool="${build_dir}/m6_task_verify"
secret_dir=${XDNA_M6_SECRET_DIR:-${repo_dir}/.local-secrets}
secret_path=${XDNA_M6_SECRET_PATH:-${secret_dir}/m6-signing-subseed}
host=${XDNA_QUBIC_NODE_HOST:-corenet.qubic.li}
port=${XDNA_QUBIC_NODE_PORT:-21841}

for required_tool in "${identity_tool}" "${authorization_tool}" "${task_tool}"; do
    if [[ ! -x "${required_tool}" ]]; then
        echo "missing ${required_tool}; configure with -DXDNA_ENABLE_PRODUCTION_CRYPTO=ON and build first" >&2
        exit 2
    fi
done
if [[ ! -f "${secret_path}" || -L "${secret_path}" ]]; then
    echo 'M6 final runner requires an explicit local identity file; no secret was loaded' >&2
    exit 2
fi

ctest --test-dir "${build_dir}" -R '^qubic_crypto_tests$' --output-on-failure
"${identity_tool}" show --path "${secret_path}"

temporary_dir=$(mktemp -d "${TMPDIR:-/tmp}/xdna-m6-final.XXXXXX")
cleanup() {
    rm -rf -- "${temporary_dir}"
}
trap cleanup EXIT

set +e
"${authorization_tool}" --secret-path "${secret_path}" --host "${host}" --port "${port}" \
    >"${temporary_dir}/authorization.out" 2>"${temporary_dir}/authorization.err"
authorization_status=$?
set -e
cat -- "${temporary_dir}/authorization.out"
cat -- "${temporary_dir}/authorization.err" >&2
authorization_result=$(sed -n '1p' "${temporary_dir}/authorization.out")
if [[ "${authorization_result}" != 'AUTHORIZED' ]]; then
    echo 'candidate_attempts=0'
    echo 'npu_score_calls=0'
    echo 'cpu_verification_count=0'
    echo 'best_finite_score=NONE'
    echo 'live_threshold=NOT_REACHED'
    echo 'submission_frame_count=0'
    echo 'submission_performed=false'
    echo 'FINAL_M6_STOPPED_BEFORE_SEARCH_AUTHORIZATION_REQUIRED'
    if [[ "${authorization_status}" == '2' ]]; then
        exit 2
    fi
    exit 3
fi

task_output=$("${repo_dir}/scripts/fetch-m6-bpp9000-task.sh")
printf '%s\n' "${task_output}"
task_path=$(sed -n 's/^task_path=//p' <<<"${task_output}")
if [[ -z "${task_path}" ]]; then
    echo 'pinned task fetch did not return a task path' >&2
    exit 3
fi
"${task_tool}" --path "${task_path}"

# The repository currently has the verified M5 single-work-item contract but
# not a production random2/candidate-orchestration runner over the pinned
# task. Stop before any search or network write instead of fabricating a
# candidate, NPU result, WorkContext refresh, signature, or submission.
echo 'candidate_attempts=0'
echo 'npu_score_calls=0'
echo 'cpu_verification_count=0'
echo 'best_finite_score=NONE'
echo 'live_threshold=NOT_REACHED'
echo 'stale_abort_count=0'
echo 'submission_frame_count=0'
echo 'submission_resend_count=0'
echo 'submission_performed=false'
echo 'FINAL_M6_NOT_READY_NO_PRODUCTION_CANDIDATE_BACKEND'
exit 2

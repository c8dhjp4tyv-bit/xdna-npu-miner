#!/usr/bin/env bash
set -euo pipefail

readonly root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly build_dir="${M3_BUILD_DIR:-${root_dir}/build}"
readonly k1_dir="${build_dir}/xdna-k1"
readonly evidence_path="${M3_EVIDENCE_PATH:-${root_dir}/docs/evidence/m3-k1-differential.json}"
readonly m2_evidence_path="${M2_EVIDENCE_PATH:-${root_dir}/docs/evidence/m2-xdna-smoke.json}"

expect_failure() {
    set +e
    "$@"
    local command_status=$?
    set -e
    if [[ "${command_status}" -eq 0 ]]; then
        printf 'EXPECTED_FAILURE_MISSING: %s\n' "$*" >&2
        exit 1
    fi
}

cmake -S "${root_dir}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Debug
cmake --build "${build_dir}" -j2
ctest --test-dir "${build_dir}" --output-on-failure
"${build_dir}/bpp9000_tests"
"${root_dir}/scripts/generate_corpus.sh" "${build_dir}"

"${root_dir}/scripts/verify-xdna1.sh" --build-dir "${build_dir}"
"${root_dir}/scripts/build-xdna-smoke.sh" "${build_dir}"
EVIDENCE_PATH="${m2_evidence_path}" BUILD_DIR="${build_dir}" \
    "${root_dir}/scripts/run-xdna-smoke.sh" --iterations 100

"${root_dir}/scripts/build-xdna-k1.sh" "${build_dir}"

"${build_dir}/xdna_k1_differential" \
    --xclbin "${k1_dir}/xdna_k1.xclbin" \
    --insts "${k1_dir}/xdna_k1.insts" \
    --artifact-sums "${k1_dir}/SHA256SUMS" \
    --fixed-count 1 \
    --random-count 0 \
    --no-edge-cases

expect_failure "${build_dir}/xdna_k1_differential" \
    --xclbin /does/not/exist \
    --insts /does/not/exist \
    --fixed-count 0 \
    --random-count 0 \
    --no-edge-cases

expect_failure "${build_dir}/xdna_k1_differential" \
    --xclbin "${k1_dir}/xdna_k1.xclbin" \
    --insts "${k1_dir}/xdna_k1.insts" \
    --selector 99 \
    --fixed-count 0 \
    --random-count 0 \
    --no-edge-cases

expect_failure "${build_dir}/xdna_k1_differential" \
    --xclbin "${build_dir}/xdna-smoke/xdna_smoke.xclbin" \
    --insts "${build_dir}/xdna-smoke/xdna_smoke.insts" \
    --manifest "${root_dir}/runtime-pins.json" \
    --fixed-count 0 \
    --random-count 0 \
    --no-edge-cases

"${build_dir}/xdna_k1_differential" \
    --xclbin "${k1_dir}/xdna_k1.xclbin" \
    --insts "${k1_dir}/xdna_k1.insts" \
    --artifact-sums "${k1_dir}/SHA256SUMS" \
    --fixed-count 100 \
    --random-count 1000 \
    --evidence "${evidence_path}" \
    --mismatch-dir "${build_dir}/m3-mismatches" \
    --m1-status PASS \
    --m2-status PASS \
    --negative-status PASS

python3 -m json.tool "${evidence_path}" >/dev/null
git -C "${root_dir}" diff --check
printf 'M3 VALIDATION PASS\n'

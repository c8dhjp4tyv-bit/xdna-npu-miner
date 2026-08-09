#!/usr/bin/env bash
set -euo pipefail

readonly root_dir="$(cd "$(dirname "$BASH_SOURCE")/.." && pwd)"
readonly build_dir="${M4_BUILD_DIR:-${root_dir}/build}"
readonly m4_dir="${build_dir}/xdna-m4"
readonly m4_evidence="${M4_EVIDENCE_PATH:-${root_dir}/docs/evidence/m4-full-score-differential.json}"
readonly m3_evidence="${build_dir}/m3-regression-evidence.json"
readonly m2_evidence="${build_dir}/m2-regression-evidence.json"

expect_contains() {
    local output="$1"
    local expected="$2"
    if [[ "${output}" != *"${expected}"* ]]; then
        printf 'EXPECTED_OUTPUT_MISSING: %s\n' "${expected}" >&2
        exit 1
    fi
}

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
m1_output="$("${build_dir}/bpp9000_tests")"
printf '%s\n' "${m1_output}"
expect_contains "${m1_output}" "generated_digest=2979889feed3352b3c12831a301a357b6c9099f3de80b955f152c53bca2f8c03"
expect_contains "${m1_output}" "production_digest=7c1da1028b9ecdbae54616654606185e62076ff7b69e209ecbf3d23f6a2fede1"
"${root_dir}/scripts/generate_corpus.sh" "${build_dir}"

"${root_dir}/scripts/verify-xdna1.sh" --build-dir "${build_dir}"
"${root_dir}/scripts/build-xdna-smoke.sh" "${build_dir}"
EVIDENCE_PATH="${m2_evidence}" BUILD_DIR="${build_dir}" \
    "${root_dir}/scripts/run-xdna-smoke.sh" --iterations 100
python3 -m json.tool "${m2_evidence}" >/dev/null

M2_EVIDENCE_PATH="${m2_evidence}" M3_EVIDENCE_PATH="${m3_evidence}" \
    "${root_dir}/scripts/run-m3-validation.sh"
python3 -m json.tool "${m3_evidence}" >/dev/null

"${root_dir}/scripts/build-xdna-m4.sh" "${build_dir}"

expect_failure "${build_dir}/xdna_m4_differential" \
    --xclbin /does/not/exist \
    --insts /does/not/exist \
    --fixed-count 0 \
    --random-count 0

expect_failure "${build_dir}/xdna_m4_differential" \
    --xclbin "${m4_dir}/xdna_m4.xclbin" \
    --insts "${m4_dir}/xdna_m4.insts" \
    --manifest "${m4_dir}/xdna_m4.manifest" \
    --selector 99 \
    --fixed-count 0 \
    --random-count 0

expect_failure "${build_dir}/xdna_m4_differential" \
    --xclbin "${m4_dir}/xdna_m4.xclbin" \
    --insts "${m4_dir}/xdna_m4.insts" \
    --manifest "${root_dir}/runtime-pins.json" \
    --fixed-count 0 \
    --random-count 0

mkdir -p "$(dirname "${m4_evidence}")"
"${build_dir}/xdna_m4_differential" \
    --xclbin "${m4_dir}/xdna_m4.xclbin" \
    --insts "${m4_dir}/xdna_m4.insts" \
    --manifest "${m4_dir}/xdna_m4.manifest" \
    --artifact-sums "${m4_dir}/SHA256SUMS" \
    --fixed-count 100 \
    --random-count 1000 \
    --random-seed 5562880460839399681 \
    --evidence "${m4_evidence}" \
    --mismatch-dir "${build_dir}/m4-mismatches" \
    --m1-status PASS \
    --m2-status PASS \
    --m3-status PASS \
    --negative-status PASS

python3 -m json.tool "${m4_evidence}" >/dev/null
git -C "${root_dir}" diff --check
printf 'M4 VALIDATION PASS\n'

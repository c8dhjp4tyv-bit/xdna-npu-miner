#!/usr/bin/env bash
set -euo pipefail

readonly root_dir="$(cd "$(dirname "$BASH_SOURCE")/.." && pwd)"
readonly build_dir="${M5_BUILD_DIR:-${root_dir}/build}"
readonly evidence_dir="${M5_CONFIG_EVIDENCE_DIR:-${build_dir}/m5-evidence}"
readonly aggregate_evidence="${M5_EVIDENCE_PATH:-${root_dir}/docs/evidence/m5-batching-four-column.json}"
readonly warmups="${M5_WARMUPS:-2}"
readonly repeats="${M5_REPEATS:-5}"
readonly work_items="${M5_WORK_ITEMS:-16}"
readonly m4_evidence="${build_dir}/m4-m5-regression-evidence.json"
readonly mismatch_dir="${build_dir}/m5-mismatches"
readonly m4_dir="${build_dir}/xdna-m4"

configs=(
    "1 1"
    "2 1"
    "4 1"
    "2 2"
    "4 2"
    "8 2"
    "4 4"
    "8 4"
    "16 4"
)

if (( work_items == 0 )); then
    printf 'INVALID_ARGUMENT: M5_WORK_ITEMS must be positive\n' >&2
    exit 2
fi
for config in "${configs[@]}"; do
    read -r batch_size columns <<<"${config}"
    if (( work_items % batch_size != 0 )); then
        printf 'INVALID_ARGUMENT: M5_WORK_ITEMS=%s is not divisible by batch size %s\n' \
            "${work_items}" "${batch_size}" >&2
        exit 2
    fi
done

cmake -S "${root_dir}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Debug
cmake --build "${build_dir}" -j2
ctest --test-dir "${build_dir}" --output-on-failure

m1_output="$("${build_dir}/bpp9000_tests")"
printf '%s\n' "${m1_output}"
[[ "${m1_output}" == *"generated_digest=2979889feed3352b3c12831a301a357b6c9099f3de80b955f152c53bca2f8c03"* ]]
[[ "${m1_output}" == *"production_digest=7c1da1028b9ecdbae54616654606185e62076ff7b69e209ecbf3d23f6a2fede1"* ]]
"${root_dir}/scripts/generate_corpus.sh" "${build_dir}"

M4_EVIDENCE_PATH="${m4_evidence}" \
    M5_BUILD_DIR="${build_dir}" \
    "${root_dir}/scripts/run-m4-validation.sh"
python3 -m json.tool "${m4_evidence}" >/dev/null

mkdir -p "${evidence_dir}"
for config in "${configs[@]}"; do
    read -r batch_size columns <<<"${config}"
    "${root_dir}/scripts/build-xdna-m5.sh" "${build_dir}" "${batch_size}" "${columns}"
done

config_evidence_args=()
for config in "${configs[@]}"; do
    read -r batch_size columns <<<"${config}"
    artifact_dir="${build_dir}/xdna-m5-b${batch_size}-c${columns}"
    evidence_path="${evidence_dir}/m5-b${batch_size}-c${columns}.json"
    "${build_dir}/xdna_m5_differential" \
        --xclbin "${artifact_dir}/xdna_m5.xclbin" \
        --insts "${artifact_dir}/xdna_m5.insts" \
        --manifest "${artifact_dir}/xdna_m5.manifest" \
        --artifact-sums "${artifact_dir}/SHA256SUMS" \
        --baseline-xclbin "${m4_dir}/xdna_m4.xclbin" \
        --baseline-insts "${m4_dir}/xdna_m4.insts" \
        --baseline-manifest "${m4_dir}/xdna_m4.manifest" \
        --batch-size "${batch_size}" \
        --columns "${columns}" \
        --warmups "${warmups}" \
        --repeats "${repeats}" \
        --work-items "${work_items}" \
        --evidence "${evidence_path}" \
        --mismatch-dir "${mismatch_dir}"
    python3 -m json.tool "${evidence_path}" >/dev/null
    config_evidence_args+=(--config "${evidence_path}")
done

python3 "${root_dir}/scripts/aggregate-m5-evidence.py" \
    --repo-root "${root_dir}" \
    --output "${aggregate_evidence}" \
    "${config_evidence_args[@]}"
python3 -m json.tool "${aggregate_evidence}" >/dev/null
git -C "${root_dir}" diff --check
printf 'M5 VALIDATION PASS\n'

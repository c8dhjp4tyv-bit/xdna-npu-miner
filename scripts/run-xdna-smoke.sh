#!/usr/bin/env bash
set -euo pipefail

readonly root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly build_dir="${BUILD_DIR:-${root_dir}/build}"
readonly artifact_dir="${ARTIFACT_DIR:-${build_dir}/xdna-smoke}"
readonly evidence_path="${EVIDENCE_PATH:-${root_dir}/docs/evidence/m2-xdna-smoke.json}"
iterations="1"
selector="0"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --iterations)
            [[ $# -ge 2 ]] || { printf 'missing value for --iterations\n' >&2; exit 2; }
            iterations="$2"
            shift 2
            ;;
        --selector)
            [[ $# -ge 2 ]] || { printf 'missing value for --selector\n' >&2; exit 2; }
            selector="$2"
            shift 2
            ;;
        --help)
            printf 'usage: %s [--iterations N] [--selector DEVICE]\n' "$0"
            exit 0
            ;;
        *)
            printf 'unknown argument: %s\n' "$1" >&2
            exit 2
            ;;
    esac
done

if [[ ! -f "${artifact_dir}/xdna_smoke.xclbin" || ! -f "${artifact_dir}/xdna_smoke.insts" ]]; then
    printf 'ARTIFACT_MISSING: build with scripts/build-xdna-smoke.sh first\n' >&2
    exit 1
fi
if [[ ! -x "${build_dir}/xdna_smoke" ]]; then
    printf 'XRT_UNAVAILABLE: build/xdna_smoke is missing; configure with XRT development files\n' >&2
    exit 1
fi

mkdir -p "$(dirname "${evidence_path}")"
"${build_dir}/xdna_smoke" \
    --xclbin "${artifact_dir}/xdna_smoke.xclbin" \
    --insts "${artifact_dir}/xdna_smoke.insts" \
    --iterations "${iterations}" \
    --selector "${selector}" \
    --evidence "${evidence_path}"
printf 'evidence=%s\n' "${evidence_path}"

#!/usr/bin/env bash
set -euo pipefail

readonly root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly default_build_dir="${BUILD_DIR:-${root_dir}/build}"
readonly xrt_root="${XILINX_XRT:-/opt/xilinx/xrt}"
build_dir="${default_build_dir}"
selector="0"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --selector)
            [[ $# -ge 2 ]] || { printf 'missing value for --selector\n' >&2; exit 2; }
            selector="$2"
            shift 2
            ;;
        --build-dir)
            [[ $# -ge 2 ]] || { printf 'missing value for --build-dir\n' >&2; exit 2; }
            build_dir="$2"
            shift 2
            ;;
        --help)
            printf 'usage: %s [--selector DEVICE] [--build-dir BUILD_DIR]\n' "$0"
            exit 0
            ;;
        *)
            printf 'unknown argument: %s\n' "$1" >&2
            exit 2
            ;;
    esac
done

if ! command -v xrt-smi >/dev/null 2>&1; then
    printf 'status=XRT_UNAVAILABLE\ndetail=xrt-smi is not available\n' >&2
    exit 1
fi
if [[ ! -d /sys/module/amdxdna && ! -d /sys/bus/pci/drivers/amdxdna ]]; then
    printf 'status=DRIVER_UNAVAILABLE\ndetail=amdxdna is not loaded\n' >&2
    exit 1
fi
if [[ ! -x "${build_dir}/xdna_probe" ]]; then
    printf 'status=TOOLCHAIN_UNAVAILABLE\ndetail=build/xdna_probe is missing; configure with XRT development files\n' >&2
    exit 1
fi

readonly mlir_aie_dir="${MLIR_AIE_DIR:-${HOME}/mlir-aie}"
if [[ ! -x "${mlir_aie_dir}/ironenv/bin/python" ]]; then
    printf 'status=TOOLCHAIN_UNAVAILABLE\ndetail=MLIR-AIE Iron interpreter is missing at %s\n' "${mlir_aie_dir}/ironenv/bin/python" >&2
    exit 1
fi
if ! (
    # shellcheck source=/dev/null
    source "${mlir_aie_dir}/ironenv/bin/activate"
    # shellcheck source=/dev/null
    source "${xrt_root}/setup.sh" >/dev/null
    export PYTHONPATH="${PYTHONPATH:-}"
    # shellcheck source=/dev/null
    source "${mlir_aie_dir}/utils/env_setup.sh" "${mlir_aie_dir}" >/dev/null
    python -c 'import aie, pyxrt' >/dev/null
    command -v aiecc.py >/dev/null
    command -v aie-opt >/dev/null
    command -v aie-translate >/dev/null
); then
    printf 'status=TOOLCHAIN_UNAVAILABLE\ndetail=MLIR-AIE/IRON imports or AIE compiler tools are unavailable\n' >&2
    exit 1
fi

set +e
probe_output="$("${build_dir}/xdna_probe" --selector "${selector}")"
probe_status=$?
set -e
printf '%s\n' "${probe_output}"

if [[ "${probe_output}" == *"status=SUPPORTED_XDNA1"* && "${probe_status}" -eq 0 ]]; then
    if [[ -f "${root_dir}/runtime-pins.json" ]]; then
        if ! command -v python3 >/dev/null 2>&1; then
            printf 'status=TOOLCHAIN_UNAVAILABLE\ndetail=python3 is required to compare runtime pins\n' >&2
            exit 1
        fi
        expected_values="$(python3 - "${root_dir}/runtime-pins.json" <<'PY'
import json
import sys

pins = json.loads(open(sys.argv[1], encoding="utf-8").read())
deps = pins["dependencies"]
print("\t".join((
    deps["amdxdna"]["value"],
    deps["npu_firmware"]["value"],
    deps["xrt"]["version"],
    deps["xrt"]["hash"],
)))
PY
)"
        IFS=$'\t' read -r expected_amdxdna expected_firmware expected_xrt expected_hash <<<"${expected_values}"
        observed_amdxdna="$(printf '%s\n' "${probe_output}" | awk -F= '$1 == "amdxdna_version" {print $2}')"
        observed_firmware="$(printf '%s\n' "${probe_output}" | awk -F= '$1 == "firmware_version" {print $2}')"
        observed_xrt="$(printf '%s\n' "${probe_output}" | awk -F= '$1 == "xrt_version" {print $2}')"
        observed_hash="$(printf '%s\n' "${probe_output}" | awk -F= '$1 == "xrt_hash" {print $2}')"
        if [[ "${observed_amdxdna}" != "${expected_amdxdna}" \
            || "${observed_firmware}" != "${expected_firmware}" \
            || "${observed_xrt}" != "${expected_xrt}" \
            || "${observed_hash}" != "${expected_hash}" ]]; then
            printf 'status=RUNTIME_VERSION_MISMATCH\nexpected_amdxdna=%s\nobserved_amdxdna=%s\nexpected_firmware=%s\nobserved_firmware=%s\nexpected_xrt=%s\nobserved_xrt=%s\nexpected_xrt_hash=%s\nobserved_xrt_hash=%s\n' \
                "${expected_amdxdna}" "${observed_amdxdna}" \
                "${expected_firmware}" "${observed_firmware}" \
                "${expected_xrt}" "${observed_xrt}" \
                "${expected_hash}" "${observed_hash}" >&2
            exit 1
        fi
    fi
    printf 'CAPABILITY PROBE PASS: physical XDNA1/AIE2 identity verified by XRT and xrt-smi\n'
    exit 0
fi
printf 'CAPABILITY PROBE FAIL CLOSED: selector=%s exit=%s\n' "${selector}" "${probe_status}" >&2
exit 1

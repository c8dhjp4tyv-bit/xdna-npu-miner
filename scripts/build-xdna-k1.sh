#!/usr/bin/env bash
set -euo pipefail

readonly root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly build_dir="${1:-${root_dir}/build}"
readonly artifact_dir="${build_dir}/xdna-k1"
readonly mlir_aie_dir="${MLIR_AIE_DIR:-${HOME}/mlir-aie}"
readonly xrt_root="${XILINX_XRT:-/opt/xilinx/xrt}"

if [[ ! -d "${mlir_aie_dir}/ironenv" ]]; then
    printf 'TOOLCHAIN_UNAVAILABLE: MLIR-AIE Iron environment not found at %s\n' "${mlir_aie_dir}" >&2
    exit 1
fi
if [[ ! -f "${xrt_root}/setup.sh" ]]; then
    printf 'XRT_UNAVAILABLE: XRT setup script not found at %s\n' "${xrt_root}" >&2
    exit 1
fi

mkdir -p "${artifact_dir}"
# shellcheck source=/dev/null
source "${mlir_aie_dir}/ironenv/bin/activate"
# shellcheck source=/dev/null
source "${xrt_root}/setup.sh" >/dev/null
# shellcheck source=/dev/null
source "${mlir_aie_dir}/utils/env_setup.sh" "${mlir_aie_dir}" >/dev/null

python "${root_dir}/src/xdna/k1_program.py" \
    --dev npu \
    --xclbin-path "${artifact_dir}/xdna_k1.xclbin" \
    --insts-path "${artifact_dir}/xdna_k1.insts"

printf '%s\n' 'artifact_kind=xdna-npu-miner-m3-k1-v1' > "${artifact_dir}/xdna_k1.manifest"

sha256sum \
    "${artifact_dir}/xdna_k1.xclbin" \
    "${artifact_dir}/xdna_k1.insts" \
    > "${artifact_dir}/SHA256SUMS"
printf 'artifact_dir=%s\n' "${artifact_dir}"
cat "${artifact_dir}/SHA256SUMS"

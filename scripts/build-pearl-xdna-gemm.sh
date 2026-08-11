#!/usr/bin/env bash
set -euo pipefail

readonly pearl_root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly pearl_build_dir="${1:-${pearl_root_dir}/build}"
readonly pearl_artifact_dir="${pearl_build_dir}/pearl-xdna-gemm-p2"
readonly pearl_mlir_aie_dir="${MLIR_AIE_DIR:-${HOME}/mlir-aie}"
readonly pearl_xrt_root="${XILINX_XRT:-/opt/xilinx/xrt}"

if [[ ! -d "${pearl_mlir_aie_dir}/ironenv" ]]; then
    printf 'TOOLCHAIN_UNAVAILABLE: MLIR-AIE Iron environment not found at %s\n' \
        "${pearl_mlir_aie_dir}" >&2
    exit 1
fi
if [[ ! -f "${pearl_xrt_root}/setup.sh" ]]; then
    printf 'XRT_UNAVAILABLE: XRT setup script not found at %s\n' "${pearl_xrt_root}" >&2
    exit 1
fi

mkdir -p "${pearl_artifact_dir}"
# shellcheck source=/dev/null
source "${pearl_mlir_aie_dir}/ironenv/bin/activate"
# shellcheck source=/dev/null
source "${pearl_xrt_root}/setup.sh" >/dev/null
# shellcheck source=/dev/null
source "${pearl_mlir_aie_dir}/utils/env_setup.sh" "${pearl_mlir_aie_dir}" >/dev/null

python "${pearl_root_dir}/src/pearl/xdna_matmul_program.py" \
    --dev npu \
    --xclbin-path "${pearl_artifact_dir}/pearl_p2_gemm.xclbin" \
    --insts-path "${pearl_artifact_dir}/pearl_p2_gemm.insts"

printf '%s\n' 'artifact_kind=pearl-xdna-gemm-p2-v1' > "${pearl_artifact_dir}/pearl_p2_gemm.manifest"
printf '%s\n' '{"schema_version":1,"artifact_kind":"pearl-xdna-gemm-p2-v1","rows":4,"common":64,"columns":8,"dtype":"int8->int32","layout":"row-major","kernel":"pearl_gemm_i8_i32","columns_used":1}' \
    > "${pearl_artifact_dir}/pearl_p2_gemm.layout.json"
sha256sum \
    "${pearl_artifact_dir}/pearl_p2_gemm.xclbin" \
    "${pearl_artifact_dir}/pearl_p2_gemm.insts" \
    > "${pearl_artifact_dir}/SHA256SUMS"
printf 'artifact_dir=%s\n' "${pearl_artifact_dir}"
cat "${pearl_artifact_dir}/SHA256SUMS"

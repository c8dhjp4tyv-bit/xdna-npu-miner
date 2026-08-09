#!/usr/bin/env bash
set -euo pipefail

readonly root_dir="$(cd "$(dirname "$BASH_SOURCE")/.." && pwd)"
readonly build_dir="${1:-${root_dir}/build}"
readonly batch_size="${2:-1}"
readonly columns="${3:-1}"
readonly artifact_dir="${build_dir}/xdna-m5-b${batch_size}-c${columns}"
readonly mlir_aie_dir="${MLIR_AIE_DIR:-${HOME}/mlir-aie}"
readonly xrt_root="${XILINX_XRT:-/opt/xilinx/xrt}"

case "${batch_size}" in
    1|2|4|8|16) ;;
    *) printf 'INVALID_ARGUMENT: unsupported M5 batch size %s\n' "${batch_size}" >&2; exit 2 ;;
esac
case "${columns}" in
    1|2|4) ;;
    *) printf 'INVALID_ARGUMENT: unsupported M5 column count %s\n' "${columns}" >&2; exit 2 ;;
esac
if (( batch_size % columns != 0 )); then
    printf 'INVALID_ARGUMENT: batch size %s is not divisible by columns %s\n' "${batch_size}" "${columns}" >&2
    exit 2
fi
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

python "${root_dir}/src/xdna/m5_program.py" \
    --dev npu \
    --batch-size "${batch_size}" \
    --columns "${columns}" \
    --xclbin-path "${artifact_dir}/xdna_m5.xclbin" \
    --insts-path "${artifact_dir}/xdna_m5.insts"

printf 'artifact_kind=xdna-npu-miner-m5-batch-v1 batch_size=%s columns=%s\n' \
    "${batch_size}" "${columns}" > "${artifact_dir}/xdna_m5.manifest"
printf '{"schema_version":1,"artifact_kind":"xdna-npu-miner-m5-batch-v1","batch_size":%s,"columns":%s,"items_per_lane":%s,"input_item_stride_bytes":15488,"output_item_stride_bytes":128,"lane_mapping":[' \
    "${batch_size}" "${columns}" "$((batch_size / columns))" > "${artifact_dir}/xdna_m5.layout.json"
for (( lane=0; lane<columns; lane++ )); do
    if (( lane > 0 )); then printf ',' >> "${artifact_dir}/xdna_m5.layout.json"; fi
    printf '{"lane":%s,"logical_column":%s,"first_item":%s,"item_count":%s}' \
        "${lane}" "${lane}" "$((lane * batch_size / columns))" "$((batch_size / columns))" \
        >> "${artifact_dir}/xdna_m5.layout.json"
done
printf '],"placement_source":"aie.mlir and main_aie_partition.json"}\n' \
    >> "${artifact_dir}/xdna_m5.layout.json"
sha256sum \
    "${artifact_dir}/xdna_m5.xclbin" \
    "${artifact_dir}/xdna_m5.insts" \
    > "${artifact_dir}/SHA256SUMS"
printf 'artifact_dir=%s\n' "${artifact_dir}"
cat "${artifact_dir}/SHA256SUMS"

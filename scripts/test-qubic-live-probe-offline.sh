#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${script_dir}/.." && pwd)
probe="${repo_dir}/build/qubic_live_probe"
if [[ ! -x "${probe}" ]]; then
    echo "missing ${probe}; build the Debug tree first" >&2
    exit 2
fi

test_tmp_dir=$(mktemp -d /tmp/xdna-qubic-live-probe.XXXXXX)
cleanup() {
    if [[ -n "${server_pid:-}" ]]; then
        kill "${server_pid}" 2>/dev/null || true
        wait "${server_pid}" 2>/dev/null || true
    fi
    rm -rf "${test_tmp_dir}"
}
trap cleanup EXIT

start_server() {
    local mode=$1
    local connections=$2
    : >"${test_tmp_dir}/port"
    python3 - "${mode}" "${connections}" >"${test_tmp_dir}/port" <<'PY' &
import socket
import struct
import sys
import time

mode = sys.argv[1]
connections = int(sys.argv[2])

def read_exact(conn, count):
    data = bytearray()
    while len(data) < count:
        chunk = conn.recv(count - len(data))
        if not chunk:
            return bytes(data)
        data.extend(chunk)
    return bytes(data)

def system_info_frame():
    payload = bytearray(128)
    struct.pack_into("<hHIIIHBBBBBBII", payload, 0,
                     301, 7, 100, 1, 99, 123, 2, 3, 4, 5, 6, 26, 11, 12)
    payload[32:64] = bytes(range(0x40, 0x60))
    struct.pack_into("<iQQIQQQQQ", payload, 64, 2, 13, 14, 15, 16, 17, 18, 19, 20)
    total = 8 + len(payload)
    return bytes((total & 0xff, (total >> 8) & 0xff, (total >> 16) & 0xff, 47)) + struct.pack("<I", 0) + payload

def read_frame(conn):
    header = read_exact(conn, 8)
    if len(header) != 8:
        return header, b''
    size = int.from_bytes(header[:3], 'little')
    return header, read_exact(conn, size - 8)

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", 0))
    server.listen(connections)
    print(server.getsockname()[1], flush=True)
    for _ in range(connections):
        conn, _ = server.accept()
        with conn:
            handshake, handshake_payload = read_frame(conn)
            if (len(handshake) != 8 or handshake[3] != 0
                    or int.from_bytes(handshake[:3], 'little') != 24
                    or len(handshake_payload) != 16):
                raise SystemExit("probe did not send EXCHANGE_PUBLIC_PEERS handshake")
            request, request_payload = read_frame(conn)
            if len(request) != 8 or request[3] != 46 or request_payload:
                raise SystemExit("probe did not send REQUEST_SYSTEM_INFO")
            if mode == "success":
                # A direct node may stream its own peer exchange and ordinary
                # broadcast traffic before the response we requested.
                unsolicited_peer = bytes((24, 0, 0, 0, 0, 0, 0, 0)) + bytes(16)
                unsolicited_broadcast = bytes((8, 0, 0, 1, 0, 0, 0, 0))
                conn.sendall(unsolicited_peer + unsolicited_broadcast + system_info_frame())
            elif mode == "wrong-frame":
                conn.sendall(bytes((8, 0, 0, 99, 0, 0, 0, 0)))
            elif mode == "truncated":
                conn.sendall(bytes((136, 0, 0, 47, 0, 0, 0, 0, 1)))
            elif mode == "timeout":
                time.sleep(1.0)
            else:
                raise SystemExit("unknown mock mode")
PY
    server_pid=$!
    for _ in $(seq 1 100); do
        if [[ -s "${test_tmp_dir}/port" ]]; then
            break
        fi
        sleep 0.01
    done
    if [[ ! -s "${test_tmp_dir}/port" ]]; then
        echo "mock server did not publish a port" >&2
        exit 1
    fi
    mock_port=$(sed -n '1p' "${test_tmp_dir}/port")
}

run_success() {
    start_server success 2
    output=$(XDNA_QUBIC_NODE_HOST=127.0.0.1 XDNA_QUBIC_NODE_PORT="${mock_port}" \
        XDNA_QUBIC_ALLOW_LIVE_SUBMISSION=0 "${probe}" --repeat 2 --attempts 1 --timeout-ms 3000)
    grep -q '^response_frame_type=47$' <<<"${output}"
    grep -q '^response_payload_bytes=128$' <<<"${output}"
    grep -q '^reconnect_pass=true$' <<<"${output}"
    grep -q '^live_submission=DISABLED$' <<<"${output}"
    wait "${server_pid}"
    unset server_pid
}

run_failure() {
    local mode=$1
    start_server "${mode}" 1
    if XDNA_QUBIC_NODE_HOST=127.0.0.1 XDNA_QUBIC_NODE_PORT="${mock_port}" \
        XDNA_QUBIC_ALLOW_LIVE_SUBMISSION=0 "${probe}" --repeat 1 --attempts 1 --timeout-ms 100 \
        >"${test_tmp_dir}/${mode}.out" 2>&1; then
        echo "${mode} mock unexpectedly passed" >&2
        exit 1
    fi
    wait "${server_pid}" 2>/dev/null || true
    unset server_pid
}

run_success
run_failure wrong-frame
run_failure truncated
run_failure timeout
echo 'PASS qubic_live_probe_offline'

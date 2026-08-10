#!/usr/bin/env bash
set -euo pipefail

readonly host="${XDNA_QUBIC_NODE_HOST:-corenet.qubic.li}"
readonly port="${XDNA_QUBIC_NODE_PORT:-21841}"
readonly deadline_ms="${XDNA_QUBIC_REQUEST_DEADLINE_MS:-15000}"
readonly read_timeout_ms="${XDNA_QUBIC_READ_TIMEOUT_MS:-3000}"
readonly max_ignored_bytes="${XDNA_QUBIC_MAX_IGNORED_BYTES:-16777216}"

# Bounded, read-only REQUEST_COMPUTORS probe. The public node can continuously
# stream valid asynchronous traffic, so success is governed by the absolute
# deadline rather than a small unsolicited-frame count.
python3 - "${host}" "${port}" "${deadline_ms}" "${read_timeout_ms}" "${max_ignored_bytes}" <<'PY'
import datetime
import hashlib
import socket
import struct
import sys
import time

host, port, deadline_ms, read_timeout_ms, max_ignored_bytes = sys.argv[1:]
port = int(port)
deadline_ms = int(deadline_ms)
read_timeout_ms = int(read_timeout_ms)
max_ignored_bytes = int(max_ignored_bytes)
if not (1 <= deadline_ms <= 120000 and 1 <= read_timeout_ms <= 60000 and 8 <= max_ignored_bytes <= 64 * 1024 * 1024):
    raise RuntimeError('invalid bounded request limits')

REQUEST_DEJAVU = 0x2468ACE0
KNOWN_UNSOLICITED = {0, 1, 2, 3, 8, 11, 14, 16, 24, 26, 27, 29, 31, 33, 36, 38, 40, 42, 44, 46, 48, 50, 52, 56, 58, 64, 66, 68, 69, 70, 201}

def frame(message_type, dejavu, payload=b''):
    size = 8 + len(payload)
    return size.to_bytes(3, 'little') + bytes([message_type]) + struct.pack('<I', dejavu) + payload

def remaining(deadline):
    value = deadline - time.monotonic()
    if value <= 0:
        raise TimeoutError('request_deadline_exceeded')
    return min(value, read_timeout_ms / 1000.0)

def read_exact(sock, size, deadline):
    data = bytearray()
    while len(data) < size:
        sock.settimeout(remaining(deadline))
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise RuntimeError('connection closed')
        data.extend(chunk)
    return bytes(data)

def read_frame(sock, deadline):
    header = read_exact(sock, 8, deadline)
    size = int.from_bytes(header[:3], 'little')
    if size < 8 or size > 0xFFFFFF:
        raise RuntimeError(f'invalid frame size {size}')
    return header[3], struct.unpack('<I', header[4:])[0], read_exact(sock, size - 8, deadline), size

addresses = []
for entry in socket.getaddrinfo(host, port, socket.AF_INET, socket.SOCK_STREAM):
    address = entry[4][0]
    if address not in addresses:
        addresses.append(address)

errors = []
for address in addresses[:32]:
    sock = None
    started = time.monotonic()
    deadline = started + deadline_ms / 1000.0
    ignored_frames = 0
    ignored_bytes = 0
    try:
        sock = socket.create_connection((address, port), timeout=remaining(deadline))
        sock.sendall(frame(0, 0x13579BDF, bytes(16)))
        sock.sendall(frame(11, REQUEST_DEJAVU))
        while True:
            message_type, dejavu, payload, size = read_frame(sock, deadline)
            if message_type == 2 and dejavu == REQUEST_DEJAVU:
                expected_payload_bytes = 2 + 676 * 32 + 64
                if len(payload) != expected_payload_bytes:
                    raise RuntimeError(f'BROADCAST_COMPUTORS payload is {len(payload)} bytes, expected {expected_payload_bytes}')
                epoch = struct.unpack_from('<H', payload, 0)[0]
                keys = payload[2:-64]
                if any(keys[index:index + 32] == bytes(32) for index in range(0, len(keys), 32)):
                    raise RuntimeError('BROADCAST_COMPUTORS contains a zero public key')
                elapsed_ms = int((time.monotonic() - started) * 1000)
                print('utc=' + datetime.datetime.now(datetime.timezone.utc).isoformat().replace('+00:00', 'Z'))
                print(f'endpoint={host}:{port}')
                print(f'responsive_address={address}')
                print('request_type=11')
                print(f'ignored_frames={ignored_frames}')
                print(f'ignored_bytes={ignored_bytes}')
                print(f'elapsed_ms={elapsed_ms}')
                print('response_frame_type=2')
                print(f'response_payload_bytes={len(payload)}')
                print(f'computors_epoch={epoch}')
                print(f'computor_count={len(keys) // 32}')
                print(f'computor_keys_sha256={hashlib.sha256(keys).hexdigest()}')
                print(f'computors_signature_sha256={hashlib.sha256(payload[-64:]).hexdigest()}')
                print('public_keys_nonzero=true')
                print('signature_verified_by_probe=false')
                raise SystemExit(0)
            if message_type not in KNOWN_UNSOLICITED:
                raise RuntimeError(f'unexpected response frame type {message_type}')
            if ignored_bytes + size > max_ignored_bytes:
                raise RuntimeError(f'ignored_byte_ceiling_exceeded ignored_frames={ignored_frames} ignored_bytes={ignored_bytes}')
            ignored_frames += 1
            ignored_bytes += size
    except Exception as error:
        elapsed_ms = int((time.monotonic() - started) * 1000)
        errors.append(f'{address}: {error}; ignored_frames={ignored_frames}; ignored_bytes={ignored_bytes}; elapsed_ms={elapsed_ms}')
    finally:
        if sock is not None:
            sock.close()

raise RuntimeError('all bounded public computor requests failed: ' + '; '.join(errors))
PY

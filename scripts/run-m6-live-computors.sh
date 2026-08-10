#!/usr/bin/env bash
set -euo pipefail

readonly host="${XDNA_QUBIC_NODE_HOST:-corenet.qubic.li}"
readonly port="${XDNA_QUBIC_NODE_PORT:-21841}"

# This is a bounded, read-only probe for the current computor destination
# source. It intentionally uses the public REQUEST_COMPUTORS protocol and
# never loads signing material or sends a BroadcastMessage.
python3 - "${host}" "${port}" <<'PY'
import datetime
import hashlib
import socket
import struct
import sys

host = sys.argv[1]
port = int(sys.argv[2])
addresses = []
for entry in socket.getaddrinfo(host, port, socket.AF_INET, socket.SOCK_STREAM):
    address = entry[4][0]
    if address not in addresses:
        addresses.append(address)

def frame(message_type, dejavu, payload=b''):
    size = 8 + len(payload)
    return size.to_bytes(3, 'little') + bytes([message_type]) + struct.pack('<I', dejavu) + payload

def read_exact(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise RuntimeError('connection closed')
        data.extend(chunk)
    return bytes(data)

def read_frame(sock):
    header = read_exact(sock, 8)
    size = int.from_bytes(header[:3], 'little')
    if size < 8 or size > 0xFFFFFF:
        raise RuntimeError(f'invalid frame size {size}')
    return header[3], struct.unpack('<I', header[4:])[0], read_exact(sock, size - 8)

errors = []
for address in addresses[:32]:
    sock = None
    try:
        sock = socket.create_connection((address, port), timeout=1.5)
        sock.settimeout(2.5)
        sock.sendall(frame(0, 0x13579BDF, bytes(16)))
        sock.sendall(frame(11, 0x2468ACE0))
        seen = []
        for _ in range(64):
            message_type, dejavu, payload = read_frame(sock)
            seen.append((message_type, len(payload)))
            if message_type != 2:
                continue
            expected_payload_bytes = 2 + 676 * 32 + 64
            if len(payload) != expected_payload_bytes:
                raise RuntimeError(
                    f'BROADCAST_COMPUTORS payload is {len(payload)} bytes, expected {expected_payload_bytes}'
                )
            epoch = struct.unpack_from('<H', payload, 0)[0]
            keys = payload[2:-64]
            if any(keys[index:index + 32] == bytes(32) for index in range(0, len(keys), 32)):
                raise RuntimeError('BROADCAST_COMPUTORS contains a zero public key')
            print('utc=' + datetime.datetime.now(datetime.timezone.utc).isoformat().replace('+00:00', 'Z'))
            print(f'endpoint={host}:{port}')
            print(f'responsive_address={address}')
            print('request_type=11')
            print(f'seen_frames={seen}')
            print('response_frame_type=2')
            print(f'response_payload_bytes={len(payload)}')
            print(f'computors_epoch={epoch}')
            print(f'computor_count={len(keys) // 32}')
            print(f'computor_keys_sha256={hashlib.sha256(keys).hexdigest()}')
            print(f'computors_signature_sha256={hashlib.sha256(payload[-64:]).hexdigest()}')
            print('public_keys_nonzero=true')
            print('signature_verified_by_probe=false')
            raise SystemExit(0)
        errors.append(f'{address}: no BROADCAST_COMPUTORS response ({seen})')
    except Exception as error:
        errors.append(f'{address}: {error}')
    finally:
        if sock is not None:
            sock.close()

raise RuntimeError('all bounded public computor requests failed: ' + '; '.join(errors))
PY

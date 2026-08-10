#!/usr/bin/env bash
set -euo pipefail

if [[ "${XDNA_QUBIC_ALLOW_LIVE_SUBMISSION:-0}" != '1' ]]; then
    echo 'LIVE_SUBMISSION_DISABLED: set XDNA_QUBIC_ALLOW_LIVE_SUBMISSION=1 for an explicit opt-in' >&2
    exit 2
fi

cat >&2 <<'EOF'
LIVE_SUBMISSION_NOT_EXERCISED_PROTOCOL_REQUIRES_AUTHORIZED_IDENTITY
No frame was sent. Current Qubic protocol requires the source to be a
computor or to satisfy the source-balance dissemination rule, and the
destination must be a current computor public key. M6 has no authorized
funded source, computor identity, destination key, valid candidate, or user
secret. This command intentionally does not start the M7 supervisor.
EOF
exit 2

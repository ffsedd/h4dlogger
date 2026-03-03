#!/usr/bin/env bash
set -euo pipefail

MOUNT="$HOME/mnt/dlogger"
REMOTE="root@10.11.12.1:/mnt/data"

mkdir -p "$MOUNT"

if ! mountpoint -q "$MOUNT"; then
    echo "mounting logger..."
    sshfs -o reconnect "$REMOTE" "$MOUNT"
fi

cd ~/dev/h4dlogger
uv run python -m h4dlogger.main

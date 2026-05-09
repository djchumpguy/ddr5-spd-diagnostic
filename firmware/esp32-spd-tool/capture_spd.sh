#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-/dev/ttyUSB0}"
BAUD=115200
OUT="spd_dump_$(date +%Y%m%d_%H%M%S).log"

echo "📥 Capturing SPD dump from $PORT"
echo "📄 Output file: $OUT"
echo "⏹  Press Ctrl+C to stop"

picocom "$PORT" -b $BAUD --imap lfcrlf | tee "$OUT"

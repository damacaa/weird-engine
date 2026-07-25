#!/bin/bash
# Generic fetch logs script
set -e

if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Usage: $0 <path_to_project> <mtp_base_path>"
    exit 1
fi

PROJECT_DIR=$(realpath "$1")
MTP_BASE="$2"
PROJECT_ID=$(basename "$PROJECT_DIR")

cd "$PROJECT_DIR"
MTP_GAME="${MTP_BASE}/ports/${PROJECT_ID}"
OUT="device-logs/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUT"

echo "== Fetching results to $PROJECT_DIR/$OUT/"
kioclient5 cp "$MTP_GAME/log.txt" "$PWD/$OUT/" || echo "no log.txt"
for f in $(kioclient5 ls "$MTP_GAME/" | grep -E '^screenshot_.*\.bmp$'); do
  kioclient5 cp "$MTP_GAME/$f" "$PWD/$OUT/" && echo "  got $f"
done

echo "== Done. Log:"
echo "------------------------------------------------------------"
cat "$OUT/log.txt" 2>/dev/null || true

#!/bin/bash
# Fetches log.txt and any screenshots from the device after a test run.

set -e
cd "$(dirname "$0")/../.."

MTP_GAME="mtp:/RG35XX-H/SD2/ports/weird-samples"
OUT="device-logs/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUT"

echo "== Fetching results to $OUT/"
kioclient5 cp "$MTP_GAME/log.txt" "$PWD/$OUT/" || echo "no log.txt"
for f in $(kioclient5 ls "$MTP_GAME/" | grep -E '^screenshot_.*\.bmp$'); do
  kioclient5 cp "$MTP_GAME/$f" "$PWD/$OUT/" && echo "  got $f"
done

echo "== Done. Log:"
echo "------------------------------------------------------------"
cat "$OUT/log.txt" 2>/dev/null || true

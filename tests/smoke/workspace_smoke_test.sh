#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <qtcode-binary>" >&2
  exit 2
fi

QTcode_BIN="$1"
DATA_DIR="$(mktemp -d)"
SMOKE_FILE="$(mktemp)"
trap 'rm -rf "$DATA_DIR" "$SMOKE_FILE"' EXIT

printf 'workspace smoke\n' >"$SMOKE_FILE"

export QT_QPA_PLATFORM=offscreen
export QTCODE_AUTO_QUIT=1
export QTCODE_WORKSPACE_SMOKE=1
export QTCODE_WORKSPACE_FILE="$SMOKE_FILE"
export XDG_DATA_HOME="$DATA_DIR"

OUTPUT="$("$QTcode_BIN" 2>&1)"
if ! grep -Fq "Workspace smoke check passed" <<<"$OUTPUT"; then
  echo "Expected workspace smoke check log" >&2
  echo "$OUTPUT" >&2
  exit 1
fi

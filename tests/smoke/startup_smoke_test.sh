#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <qtcode-binary>" >&2
  exit 2
fi

QTcode_BIN="$1"
DATA_DIR="$(mktemp -d)"
trap 'rm -rf "$DATA_DIR"' EXIT

export QT_QPA_PLATFORM=offscreen
export QTCODE_AUTO_QUIT=1
export XDG_DATA_HOME="$DATA_DIR"

"$QTcode_BIN"

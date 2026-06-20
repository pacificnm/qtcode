#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 ]]; then
  echo "Usage: $0 <qtcode-binary> <repository-path> <sqlite3-binary>" >&2
  exit 2
fi

QTcode_BIN="$1"
REPOSITORY_PATH="$2"
SQLITE3_BIN="$3"
DATA_DIR="$(mktemp -d)"
trap 'rm -rf "$DATA_DIR"' EXIT

export QT_QPA_PLATFORM=offscreen
export QTCODE_AUTO_QUIT=1
export XDG_DATA_HOME="$DATA_DIR"

export QTCODE_ADD_REPOSITORY="$REPOSITORY_PATH"
"$QTcode_BIN"

DB_PATH="$DATA_DIR/QTCode/qtcode/qtcode.db"
if [[ ! -f "$DB_PATH" ]]; then
  echo "Expected SQLite database at $DB_PATH" >&2
  exit 1
fi

PROJECT_COUNT="$("$SQLITE3_BIN" "$DB_PATH" "SELECT COUNT(*) FROM projects;")"
if [[ "$PROJECT_COUNT" != "1" ]]; then
  echo "Expected one project row, found $PROJECT_COUNT" >&2
  exit 1
fi

ACTIVE_PROJECT_ID="$("$SQLITE3_BIN" "$DB_PATH" "SELECT value_json FROM settings WHERE key='app.active_project_id';")"
if [[ -z "$ACTIVE_PROJECT_ID" ]]; then
  echo "Expected app.active_project_id setting to be saved" >&2
  exit 1
fi

unset QTCODE_ADD_REPOSITORY
OUTPUT="$("$QTcode_BIN" 2>&1)"
if ! grep -Fq "Restored active project" <<<"$OUTPUT"; then
  echo "Expected active project restore log on second launch" >&2
  echo "$OUTPUT" >&2
  exit 1
fi

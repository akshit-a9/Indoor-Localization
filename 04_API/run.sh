#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ -f "$REPO_ROOT/.venv/Scripts/activate" ]; then
  # shellcheck disable=SC1091
  source "$REPO_ROOT/.venv/Scripts/activate"
elif [ -f "$REPO_ROOT/.venv/bin/activate" ]; then
  # shellcheck disable=SC1091
  source "$REPO_ROOT/.venv/bin/activate"
else
  echo "No virtualenv found at $REPO_ROOT/.venv" >&2
  exit 1
fi

cd "$SCRIPT_DIR"
exec uvicorn app.main:app --host 0.0.0.0 --port 8000

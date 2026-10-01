#!/usr/bin/env bash
#
# serve.sh - Run the MkDocs development server with live reload.
#
# Usage:
#   ./docs/serve.sh            # serve on http://127.0.0.1:8000
#   ./docs/serve.sh -a 0.0.0.0:8080   # pass extra args through to mkdocs
#
set -euo pipefail

# Resolve the repository root (parent of this script's directory).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$REPO_ROOT"

# Activate the local virtual environment if present.
if [[ -f ".venv/bin/activate" ]]; then
  # shellcheck disable=SC1091
  source ".venv/bin/activate"
fi

# Ensure mkdocs is available.
if ! command -v mkdocs >/dev/null 2>&1; then
  echo "mkdocs is not installed. Install the docs dependencies first:" >&2
  echo "  python3 -m venv .venv && source .venv/bin/activate" >&2
  echo "  pip install -r docs/requirements.txt" >&2
  exit 1
fi

# Start the live-reload dev server, forwarding any extra arguments.
exec mkdocs serve "$@"

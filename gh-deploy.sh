#!/usr/bin/env bash
#
# gh-deploy.sh - Build and publish the documentation to GitHub Pages.
#
# This runs `mkdocs gh-deploy`, which builds the site and force-pushes the
# result to the `gh-pages` branch of the origin remote. GitHub Pages then
# serves it at https://displaylink.github.io/displaylink-direct/.
#
# Usage:
#   ./docs/gh-deploy.sh              # build and deploy to gh-pages
#   ./docs/gh-deploy.sh --dirty      # pass extra args through to mkdocs gh-deploy
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

# Build the docs and force-push them to the gh-pages branch.
exec mkdocs gh-deploy -r syna-isd --clean "$@"

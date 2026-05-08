#!/usr/bin/env bash
set -euo pipefail

git init
git add .
git commit -m "docs: seed DDR5 SPD diagnostic notes"

echo
echo "Local repo initialized."
echo "Next: create a GitHub repo and add it as origin, for example:"
echo "  gh repo create ddr5-spd-diagnostic-notes --private --source=. --remote=origin --push"

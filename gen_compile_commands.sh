#!/usr/bin/env bash
set -euo pipefail

# 1) Generate compile_commands.json with bear
bear -- make build -B

# 2) Remove unwanted flags from compile_commands.json using external script
python3 tools/cc_modifier/cc_modifier.py compile_commands.json

# 3) Move to .vscode/
mkdir -p .vscode
mv -f compile_commands.json .vscode/compile_commands.json

echo "Generated and moved .vscode/compile_commands.json"

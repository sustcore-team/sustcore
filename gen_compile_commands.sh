#!/usr/bin/env bash
set -euo pipefail

# 1) Generate compile_commands.json with bear
bear -- make build -B

# 2) Remove unwanted flags from compile_commands.json
python3 - <<'PY'
import json
import shlex
from pathlib import Path

path = Path('compile_commands.json')
with path.open('r', encoding='utf-8') as f:
    data = json.load(f)

remove_flags = {'-fno-toplevel-reorder', '-fno-tree-scev-cprop'}

def clean_args(args):
    return [a for a in args if a not in remove_flags]

def clean_command(cmd):
    try:
        parts = shlex.split(cmd)
    except ValueError:
        # If splitting fails, fallback to simple replace
        for flag in remove_flags:
            cmd = cmd.replace(f' {flag} ', ' ')
            cmd = cmd.replace(f' {flag}', '')
        return cmd
    parts = [p for p in parts if p not in remove_flags]
    return ' '.join(shlex.quote(p) for p in parts)

for entry in data:
    if 'arguments' in entry and isinstance(entry['arguments'], list):
        entry['arguments'] = clean_args(entry['arguments'])
    if 'command' in entry and isinstance(entry['command'], str):
        entry['command'] = clean_command(entry['command'])

with path.open('w', encoding='utf-8') as f:
    json.dump(data, f, ensure_ascii=False, indent=2)
    f.write('\n')
PY

# 3) Move to .vscode/
mkdir -p .vscode
mv -f compile_commands.json .vscode/compile_commands.json

echo "Generated and moved .vscode/compile_commands.json"

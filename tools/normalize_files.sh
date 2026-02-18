#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   tools/normalize_files.sh data/imported
#   tools/normalize_files.sh data/import
#
# Writes normalized output in-place (creates .bak backups).
# Safe for git: you can diff after.

DIR="${1:-}"
if [[ -z "$DIR" || ! -d "$DIR" ]]; then
  echo "Usage: $0 <dir_with_txt_files>"
  exit 1
fi

python3 - <<'PY' "$DIR"
import re, sys, pathlib

root = pathlib.Path(sys.argv[1])

def norm_bool(s: str):
    s = s.strip().lower()
    if s in ("true","1","yes"): return "true"
    if s in ("false","0","no"): return "false"
    return None

def normalize_text(text: str) -> str:
    lines = text.splitlines()

    out = []
    for line in lines:
        # preserve original line for some transforms
        raw = line

        # Trim trailing whitespace
        line = line.rstrip()

        # Normalize InTavern=... (common tail line)
        m = re.match(r'^\s*InTavern\s*=\s*(true|false|1|0|yes|no)\s*$', line, re.I)
        if m:
            out.append(f"IN_TAVERN: {norm_bool(m.group(1))}")
            continue

        # Normalize "IN_TAVERN:" casing to exact
        m = re.match(r'^\s*IN[_ ]TAVERN\s*:\s*(.+)$', line, re.I)
        if m:
            b = norm_bool(m.group(1))
            if b is not None:
                out.append(f"IN_TAVERN: {b}")
            else:
                out.append("IN_TAVERN: " + m.group(1).strip())
            continue

        # Normalize SPECIALTY header variant "L5CompletedTotal:" -> "L5:"
        line = re.sub(r'^(SPECIALTY\s+\d+\s+L5)CompletedTotal\s*:\s*$',
                      r'\1:', line, flags=re.I)

        # Normalize specialty header case: "Specialty" -> "SPECIALTY"
        # (doesn't change comments like "# SPECIALTY 1 | ...")
        m = re.match(r'^\s*(specialty)\s+(\d+)\s+(L\d+.*)$', line, re.I)
        if m:
            line = f"SPECIALTY {m.group(2)} {m.group(3)}"
            # keep original suffix unchanged (e.g., "L5:")

        # Normalize ROLE/GENERAL to strict "GENERAL:" etc. if present
        m = re.match(r'^\s*GENERAL\s*:\s*(.+)$', line, re.I)
        if m:
            out.append("GENERAL: " + m.group(1).strip())
            continue

        m = re.match(r'^\s*ROLE\s*:\s*(.+)$', line, re.I)
        if m:
            out.append("ROLE: " + m.group(1).strip())
            continue

        # Keep as-is otherwise
        out.append(line)

    # Collapse >2 blank lines to 1
    cleaned = []
    blank_run = 0
    for l in out:
        if l.strip() == "":
            blank_run += 1
            if blank_run <= 1:
                cleaned.append("")
        else:
            blank_run = 0
            cleaned.append(l)

    # Ensure file ends with newline
    return "\n".join(cleaned).rstrip() + "\n"


txts = sorted(root.glob("*.txt"))
if not txts:
    print(f"No .txt files found in {root}")
    sys.exit(0)

for p in txts:
    orig = p.read_text(encoding="utf-8", errors="replace")
    new = normalize_text(orig)
    if new != orig:
        bak = p.with_suffix(p.suffix + ".bak")
        if not bak.exists():
            bak.write_text(orig, encoding="utf-8")
        p.write_text(new, encoding="utf-8")
        print(f"normalized: {p.name}")
PY

echo "Done."

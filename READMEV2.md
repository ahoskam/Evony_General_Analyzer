# Evony General Analyzer — GUI v2 (minimal skeleton)

This is a **minimal, working GUI skeleton** for your Evony General Analyzer that implements:

- General list with search + role filter
- Strict **dirty state** prompt (Save / Discard / Cancel) when switching generals
- **Locking** behavior: when `double_checked_in_game=1`, everything becomes read-only (except unlock checkbox)
- Meta editor: role dropdown, role_confirmed, in_tavern, base_skill_name, core stats + greens
- Source text display: multiline read-only
- Stat occurrences editor grouped by (context_type, context_name, level)
- Provenance shown (file_path:line_number, raw_line)
- New columns supported: `origin`, `generated_from_total_id`, `edited_by_user`
- Save in a **single transaction**, followed by reload from DB
- Specialty validation: blocks save if sum(levels) != TOTAL for same (context_name + stat_key_id)

## Assumptions

- You already have your project’s Dear ImGui + GLFW + OpenGL3 setup available.
- You can include ImGui headers and link glfw + OpenGL + sqlite3.
- Your DB path defaults to `data/evony_v2.db` (override with `--db path`)

## Build (example)

The current repo build path is the top-level `Makefile`.

```bash
make gui
./build/evony_gui_v2 --db data/evony_v2.db
```

## Important

This is a skeleton meant to drop into your repo and adapt to your existing file layout.
If you want me to wire it into your existing Makefile + folder structure, tell me:
- where your imgui sources live
- how you currently build the GUI binary

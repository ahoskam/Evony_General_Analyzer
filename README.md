# Evony General Analyzer

Evony General Analyzer is a desktop application for exploring, editing, and comparing **Evony: The King’s Return** generals, their buffs, and build configurations.

This project is intentionally built as an experiment in **“vibe coding”** — human-guided, AI-assisted development.

> **Primary implementation:** ChatGPT (OpenAI)  
> **Guidance, architecture, game knowledge, and final decisions:** GitHub user **AHoskam**

The codebase reflects an iterative back‑and‑forth between human intent and AI-generated code, with frequent inspection, correction, and restructuring by the maintainer.

---

## ⚠️ Project Status

🚧 **Active / Experimental**

This tool is usable, but not complete.  
Expect rough edges, unfinished features, and evolving design decisions.

The repository is public primarily for:
- learning
- transparency
- experimentation
- documentation of an AI-assisted development workflow

---

## ✨ Currently Working Features

### General Management
- Add generals (name + role: Ground / Mounted / Ranged / Siege / Other)
- Persist generals in a local SQLite database
- Select generals from a live list

[screen shot of add general tab]  
[screen shot of general list panel]

---

### Modifier System
- Add stat modifiers manually
- Supported modifier sources:
  - Base
  - Ascension
  - Specialty
  - Covenant
- All modifiers are stored in normalized form in SQLite

[screen shot of add modifier panel]  
[screen shot of modifier table]

---

### Stat Computation
- Aggregates stats from all active modifiers
- **Effective troop stats**:
  - Base troop ATK / DEF / HP automatically include matching  
    `Attacking[Troop][ATK/DEF/HP]` bonuses
  - Database keys remain distinct — merging happens only in calculations
- Zero‑value stats are hidden to reduce noise

[screen shot of computed stats panel]

---

### GUI
- Built with **Dear ImGui**
- Tab‑based navigation:
  - Home
  - Compare Generals
  - Add General
  - Edit General (partial / in progress)

[screen shot of home tab]  
[screen shot of compare generals tab]

---

## 🚫 Not Implemented Yet

- Side‑by‑side general comparison
- Importing generals from CSV / text files
- Full Edit‑General workflow (currently read‑only)
- Complete validation of in‑game combat formulas
- UI polish / theming

These are planned, but not guaranteed.

---

## 🛠️ Building the Project

### Common Requirements (All Platforms)

- C++17 compatible compiler
- SQLite3
- OpenGL
- GLFW
- Dear ImGui (included in repository)

---

## 🐧 Building on Linux (Tested)

Tested on **Manjaro / Arch Linux**.

### Install dependencies (Arch example)
```bash
sudo pacman -S base-devel sqlite glfw-x11 mesa
```

### Build
From the repository root:
```bash
make
```

or, if your Makefile supports it:
```bash
make gui
```

### Run
```bash
./build/evony_generals_gui
```

If running under Wayland:
```bash
env GLFW_PLATFORM=x11 ./build/evony_generals_gui
```

---

## 🪟 Building on Windows

Windows is supported, but not the primary development target.

### Option 1: MSYS2 (Recommended)

1. Install **MSYS2**
2. Use the **MinGW64 shell**
3. Install dependencies:
   ```bash
   pacman -S mingw-w64-x86_64-gcc \
              mingw-w64-x86_64-sqlite3 \
              mingw-w64-x86_64-glfw
   ```
4. Build using the provided Makefile  
   (minor path tweaks may be required)

---

### Option 2: Visual Studio

1. Create a new C++ project
2. Add all `.cpp` and `.h` files
3. Link against:
   - `sqlite3`
   - `glfw3`
   - `opengl32.lib`
4. Set language standard to **C++17**

Note: the project structure is Linux‑first.

---

## 🗄️ Database

- Uses SQLite (`evony.db`)
- Schema is auto‑created / synchronized at startup
- Stat keys are stored in the database and mapped in code
- The database is **intentionally not auto‑merged or auto‑normalized**
  beyond key identity — correctness is prioritized over convenience

---

## 🤝 Philosophy & Credits

This project intentionally explores:
- AI‑assisted programming
- Human‑in‑the‑loop design
- Transparent attribution of AI contributions
- Learning through iteration, not perfection

> ChatGPT is treated as a **collaborator**, not an authority.  
> All final technical decisions are made by the human maintainer.

**Maintainer:**  
GitHub: https://github.com/AHoskam

---

## 📜 License

License not yet selected.

At present, this project is shared for educational and experimental purposes.

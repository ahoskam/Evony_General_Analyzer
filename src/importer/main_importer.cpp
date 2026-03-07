// src/importer/main_importer.cpp

#include "DbImportV2.h"
#include "GeneralLoaderV2.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

static bool looks_like_backup_file(const fs::path &p) {
  const auto fn = p.filename().string();
  if (fn.find(".bak") != std::string::npos)
    return true;
  if (fn.find("~") != std::string::npos)
    return true;
  return false;
}

static bool is_in_bak_dir(const fs::path &p) {
  for (const auto &part : p) {
    if (part == "_bak")
      return true;
  }
  return false;
}

static std::string now_stamp_YYYYMMDD_HHMMSS() {
  using namespace std::chrono;
  const auto t = system_clock::to_time_t(system_clock::now());
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                tm.tm_min, tm.tm_sec);
  return buf;
}

// Moves file into dest_dir, preserving name, and adding timestamp if collision.
// Never throws; prints error and returns false.
static bool safe_move(const fs::path &src, const fs::path &dest_dir) {
  try {
    fs::create_directories(dest_dir);

    fs::path dst = dest_dir / src.filename();
    if (fs::exists(dst)) {
      // add timestamp before extension
      const auto stem = dst.stem().string();
      const auto ext = dst.extension().string();
      dst = dest_dir / (stem + "_" + now_stamp_YYYYMMDD_HHMMSS() + ext);
    }

    fs::rename(src, dst);
    return true;
  } catch (const std::exception &e) {
    std::cerr << "Move failed for " << src << ": " << e.what() << "\n";
    return false;
  }
}

// ---------------- Specialty expansion patterns (ABS totals) ----------------
// Returns per-level increments [L1..L5] that sum to abs_total.
static std::optional<std::array<int, 5>>
split_specialty_total_abs(int abs_total) {
  switch (abs_total) {
  case 6:
    return std::array<int, 5>{1, 1, 1, 1, 2}; // Pattern B
  case 10:
    return std::array<int, 5>{1, 1, 2, 2, 4}; // Pattern A
  case 15:
    return std::array<int, 5>{1, 2, 3, 3, 6};
  case 16:
    return std::array<int, 5>{2, 2, 3, 3, 6};
  case 20:
    return std::array<int, 5>{2, 2, 4, 4, 8};
  case 25:
    return std::array<int, 5>{2, 3, 5, 5, 10};
  case 26:
    return std::array<int, 5>{3, 3, 5, 5, 10};
  case 30:
    return std::array<int, 5>{3, 3, 6, 6, 12};
  case 36:
    return std::array<int, 5>{4, 4, 7, 7, 14};
  case 45:
    return std::array<int, 5>{4, 5, 9, 9, 18};
  case 35:
    return std::array<int, 5>{3, 4, 6, 7, 15};
  case 40:
    return std::array<int, 5>{4, 4, 8, 8, 16};
  case 46:
    return std::array<int, 5>{5, 5, 9, 9, 18};
  case 50:
    return std::array<int, 5>{5, 5, 10, 10, 20}; // Pattern C
  default:
    return std::nullopt;
  }
}

static bool is_intish(double v) { return std::fabs(v - std::round(v)) < 1e-9; }

static int parse_first_int(const std::string &s) {
  int v = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    if (std::isdigit((unsigned char)s[i])) {
      v = 0;
      while (i < s.size() && std::isdigit((unsigned char)s[i])) {
        v = v * 10 + (s[i] - '0');
        ++i;
      }
      return v;
    }
  }
  return 0;
}

// If a specialty+stat has ONLY a single L5 row and no L1-L4 rows, treat it as
// TOTAL. This matches Evony's "Max Level Attributes" files where only L5 is
// shown.
static void promote_singleton_specialty_l5_to_total(LoadedGeneralV2 &g) {
  struct Key {
    std::string raw_key;
    int spec_n = 0;
    bool operator==(const Key &o) const {
      return raw_key == o.raw_key && spec_n == o.spec_n;
    }
  };
  struct KeyHash {
    size_t operator()(const Key &k) const {
      size_t h1 = std::hash<std::string>{}(k.raw_key);
      size_t h2 = std::hash<int>{}(k.spec_n);
      return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
  };

  struct Agg {
    int count_non_total_levels =
        0; // count of is_total==false specialty rows with level
    bool has_level_1to4 = false;
    bool has_total = false;
    int idx_l5 =
        -1; // index in g.occurrences of the lone L5 row (if applicable)
  };

  std::unordered_map<Key, Agg, KeyHash> agg;
  agg.reserve(256);

  for (int i = 0; i < (int)g.occurrences.size(); ++i) {
    const auto &o = g.occurrences[i];
    if (o.context_type != "Specialty")
      continue;
    if (!o.level.has_value())
      continue;

    const int spec_n = parse_first_int(o.context_name);
    if (spec_n <= 0)
      continue;

    Key k{o.raw_key, spec_n};
    auto &a = agg[k];

    if (o.is_total) {
      a.has_total = true;
      continue;
    }

    const int lv = *o.level;
    if (lv >= 1 && lv <= 4)
      a.has_level_1to4 = true;

    if (lv >= 1 && lv <= 5) {
      a.count_non_total_levels++;
      if (lv == 5)
        a.idx_l5 = i;
      else
        a.idx_l5 = -1; // if we see any non-L5 row, it's not a singleton
    }
  }

  for (const auto &[k, a] : agg) {
    if (a.has_total)
      continue;
    if (a.has_level_1to4)
      continue;

    // singleton: exactly one row, and it's L5
    if (a.count_non_total_levels == 1 && a.idx_l5 >= 0) {
      auto &o = g.occurrences[a.idx_l5];
      o.is_total = true;
      o.context_name = "SPECIALTY " + std::to_string(k.spec_n);
      o.raw_line = "PROMOTED_SINGLETON_L5_TO_TOTAL (Max Level Attributes file)";
    }
  }
}

// If specialties only have L5 TOTAL rows (is_total=1, level=5) and no L1-4
// rows, synthesize increment rows for L1-4 and L5 (non-total increment),
// keeping original TOTAL row.
static void synthesize_specialty_levels_if_needed(LoadedGeneralV2 &g) {
  // We need to detect whether L1-4 exists already for a given (raw_key,
  // context_type, context_name). If yes, do nothing.
  struct Key {
    std::string raw_key;
    std::string context_type;
    std::string context_name;
    bool operator==(const Key &o) const {
      return raw_key == o.raw_key && context_type == o.context_type &&
             context_name == o.context_name;
    }
  };
  struct KeyHash {
    size_t operator()(const Key &k) const {
      std::hash<std::string> h;
      size_t x = h(k.raw_key);
      x ^= (h(k.context_type) << 1);
      x ^= (h(k.context_name) << 2);
      return x;
    }
  };

  std::unordered_map<Key, bool, KeyHash> has_any_level_1to4;
  std::unordered_map<Key, bool, KeyHash> has_level5_increment;

  for (const auto &o : g.occurrences) {
    if (o.context_type != "Specialty")
      continue;
    if (!o.level.has_value())
      continue;

    Key k{o.raw_key, o.context_type, o.context_name};

    if (*o.level >= 1 && *o.level <= 4) {
      has_any_level_1to4[k] = true;
    }
    if (*o.level == 5 && o.is_total == false) {
      has_level5_increment[k] = true;
    }
  }

  std::vector<StatOccurrenceV2> synth;

  for (const auto &o : g.occurrences) {
    if (o.context_type != "Specialty")
      continue;
    if (!o.level.has_value() || *o.level != 5)
      continue;
    if (!o.is_total)
      continue; // only synth based on TOTAL rows

    if (!is_intish(o.value))
      continue;

    const int abs_total = (int)std::llround(std::fabs(o.value));
    const auto split = split_specialty_total_abs(abs_total);
    if (!split.has_value())
      continue;

    Key k{o.raw_key, o.context_type, o.context_name};

    // If already expanded, do nothing.
    if (has_any_level_1to4[k])
      continue;
    if (has_level5_increment[k])
      continue;

    const double sign = (o.value < 0) ? -1.0 : 1.0;

    // Create L1-L4 increment rows
    for (int lvl = 1; lvl <= 4; ++lvl) {
      StatOccurrenceV2 o2 = o;
      o2.level = lvl;
      o2.is_total = false;
      o2.value = sign * (double)(*split)[lvl - 1];

      // Make it obvious this is synthetic
      {
        std::ostringstream oss;
        oss << "SYNTH_SPECIALTY_L" << lvl << " from TOTAL " << abs_total;
        o2.raw_line = oss.str();
      }
      // file_path: keep original o.file_path (already set by loader)
      // line_number: keep original (or 0 if you prefer)
      synth.push_back(std::move(o2));
    }

    // Create L5 increment row (non-total) using split[4]
    {
      StatOccurrenceV2 o2 = o;
      o2.level = 5;
      o2.is_total = false;
      o2.value = sign * (double)(*split)[4];
      {
        std::ostringstream oss;
        oss << "SYNTH_SPECIALTY_L5_INCREMENT from TOTAL " << abs_total;
        o2.raw_line = oss.str();
      }
      synth.push_back(std::move(o2));
    }
  }

  if (!synth.empty()) {
    g.occurrences.insert(g.occurrences.end(), synth.begin(), synth.end());
  }
}

// If specialties have level rows (is_total=0, levels 1..5) but no L5 TOTAL
// (is_total=1), create a synthetic TOTAL row whose value is the sum of the
// level rows.
//
// IMPORTANT HEURISTIC:
// - If we only see a single level=5 row and nothing else, we do NOT auto-create
// a TOTAL,
//   because that row could already represent a total (ambiguous).
static void synthesize_specialty_totals_if_missing(LoadedGeneralV2 &g) {
  struct Key {
    std::string raw_key;
    int spec_n = 0;
    bool operator==(const Key &o) const {
      return raw_key == o.raw_key && spec_n == o.spec_n;
    }
  };
  struct KeyHash {
    size_t operator()(const Key &k) const {
      std::hash<std::string> h;
      size_t x = h(k.raw_key);
      x ^= (std::hash<int>{}(k.spec_n) << 1);
      return x;
    }
  };

  struct Agg {
    double sum = 0.0;
    int count = 0;
    bool has_level_1to4 = false;
    bool has_total = false;
    std::string file_path;
    int line_number = 0;
  };

  std::unordered_map<Key, Agg, KeyHash> agg;
  agg.reserve(128);

  for (const auto &o : g.occurrences) {
    if (o.context_type != "Specialty")
      continue;
    if (!o.level.has_value())
      continue;

    const int spec_n = parse_first_int(o.context_name);
    if (spec_n <= 0)
      continue;

    Key k{o.raw_key, spec_n};
    auto &a = agg[k];

    if (a.file_path.empty()) {
      a.file_path = o.file_path;
      a.line_number = o.line_number;
    }

    if (o.is_total) {
      a.has_total = true;
      continue;
    }

    const int lv = *o.level;
    if (lv >= 1 && lv <= 4)
      a.has_level_1to4 = true;
    if (lv >= 1 && lv <= 5) {
      a.sum += o.value;
      a.count++;
    }
  }

  std::vector<StatOccurrenceV2> synth;
  synth.reserve(64);

  for (const auto &[k, a] : agg) {
    if (a.has_total)
      continue;
    if (a.count <= 0)
      continue;

    // Avoid the ambiguous case: only one L5 row.
    if (!a.has_level_1to4 && a.count == 1)
      continue;

    StatOccurrenceV2 o2;
    o2.raw_key = k.raw_key;
    o2.value = a.sum;
    o2.context_type = "Specialty";
    o2.context_name = "SPECIALTY " + std::to_string(k.spec_n) + " L5 (TOTAL)";
    o2.level = 5;
    o2.is_total = true;
    o2.file_path = a.file_path;
    o2.line_number = a.line_number;
    o2.raw_line = "SYNTH_SPECIALTY_TOTAL from L1-L5 level rows";

    synth.push_back(std::move(o2));
  }

  if (!synth.empty()) {
    g.occurrences.insert(g.occurrences.end(), synth.begin(), synth.end());
  }
}

int main(int argc, char **argv) {
  std::string db_path;
  std::string import_path;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--db" && i + 1 < argc)
      db_path = argv[++i];
    else if (a == "--path" && i + 1 < argc)
      import_path = argv[++i];
  }

  if (db_path.empty() || import_path.empty()) {
    std::cerr << "Usage: importer_v2 --db <dbfile> --path <dir>\n";
    return 1;
  }

  const fs::path import_dir = fs::path(import_path);
  const fs::path data_dir = import_dir.parent_path(); // e.g. data/
  const fs::path imported_dir = data_dir / "imported";
  const fs::path invalid_dir = data_dir / "_invalid";

  fs::create_directories(imported_dir);
  fs::create_directories(invalid_dir);

  DbImportV2 db;
  if (!db.open(db_path))
    return 1;

  int generals = 0;
  int occ_inserted = 0;
  int pending_examples = 0;

  // ---- Pass 1: snapshot top-level files only (NO recursion) ----
  std::vector<fs::path> files;
  try {
    for (auto const &ent : fs::directory_iterator(import_dir)) {
      if (!ent.is_regular_file())
        continue;

      const auto p = ent.path();
      if (p.extension() != ".txt")
        continue;

      // ✅ do NOT import anything in /_bak/
      if (is_in_bak_dir(p))
        continue;

      // ✅ do NOT import copied clones
      if (looks_like_backup_file(p))
        continue;

      files.push_back(p);
    }
  } catch (const std::exception &e) {
    std::cerr << "Directory scan error: " << e.what() << "\n";
    return 1;
  }

  std::sort(files.begin(), files.end());

  // ---- Pass 2: process snapshot safely ----
  try {
    for (const auto &p : files) {

      LoadedGeneralV2 g = load_general_v2_from_file(p.string());
      if (!g.errors.empty()) {
        std::cerr << "ERROR in " << p << "\n";
        for (auto &e : g.errors)
          std::cerr << "  " << e << "\n";
        safe_move(p, invalid_dir);
        continue;
      }

      // ✅ If file uses "L5 only" Max Level Attributes specialty format, treat
      // L5 as TOTAL.
      promote_singleton_specialty_l5_to_total(g);

      // ✅ Expand specialties on-the-fly during import if needed (TOTAL ->
      // L1..L5)
      synthesize_specialty_levels_if_needed(g);

      // ✅ If file has Specialty L1..L5 rows but no TOTAL row, synthesize TOTAL
      // (sum of levels)
      synthesize_specialty_totals_if_missing(g);

      if (!db.begin())
        return 1;

      int general_id = -1;
      if (!db.upsert_general(g.meta.name, g.meta.role, g.meta.country,
                             g.meta.has_covenant, g.meta.covenant_member_1,
                             g.meta.covenant_member_2,
                             g.meta.covenant_member_3,
                             /*role_confirmed*/ (g.meta.role != "Unknown"),
                             g.meta.in_tavern, g.meta.base_skill_name,
                             g.meta.leadership, g.meta.leadership_green,
                             g.meta.attack, g.meta.attack_green, g.meta.defense,
                             g.meta.defense_green, g.meta.politics,
                             g.meta.politics_green, g.meta.source_text_verbatim,
                             g.meta.double_checked_in_game, general_id)) {
        db.rollback();
        safe_move(p, invalid_dir);
        continue;
      }

      // idempotent per file path (as originally imported)
      db.delete_occurrences_for_general_file(general_id, p.string());

      for (auto const &o : g.occurrences) {
        auto stat_id = db.resolve_stat_key_id(o.raw_key);
        if (!stat_id) {
          DbImportV2::PendingInfo pi;
          if (db.ensure_pending_key(o.raw_key, o.file_path, o.line_number,
                                    pi)) {
            db.add_pending_example(pi.pending_id, g.meta.name, o.context_type,
                                   o.context_name, o.level, o.value,
                                   o.file_path, o.line_number, o.raw_line);
            pending_examples++;
          }
          continue;
        }

        if (db.insert_stat_occurrence(
                general_id, *stat_id, o.value, o.context_type, o.context_name,
                o.level, o.is_total, o.file_path, o.line_number, o.raw_line)) {
          occ_inserted++;
        }
      }

      if (!db.commit()) {
        db.rollback();
        safe_move(p, invalid_dir);
        continue;
      }

      generals++;

      // ✅ move successful file to imported (after commit)
      safe_move(p, imported_dir);
    }

  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
    return 1;
  }

  std::cout << "Importer v2 finished.\n"
            << "  Generals imported/updated: " << generals << "\n"
            << "  Stat occurrences inserted: " << occ_inserted << "\n"
            << "  Pending examples inserted: " << pending_examples << "\n";

  return 0;
}

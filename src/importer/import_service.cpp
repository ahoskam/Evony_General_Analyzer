#include "import_service.h"

#include "DbImportV2.h"
#include "GeneralLoaderV2.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {

template <typename ResultT>
void add_message(ResultT& result, const std::string& msg) {
  constexpr size_t kMaxMessages = 100;
  if (result.messages.size() < kMaxMessages) {
    result.messages.push_back(msg);
  } else if (result.messages.size() == kMaxMessages) {
    result.messages.push_back("Additional import messages truncated.");
  }
}

bool looks_like_backup_file(const fs::path& p) {
  const auto fn = p.filename().string();
  return fn.find(".bak") != std::string::npos || fn.find("~") != std::string::npos;
}

bool is_in_bak_dir(const fs::path& p) {
  for (const auto& part : p) {
    if (part == "_bak") {
      return true;
    }
  }
  return false;
}

std::string now_stamp_YYYYMMDD_HHMMSS() {
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

bool safe_move(const fs::path& src, const fs::path& dest_dir, std::string* err) {
  try {
    fs::create_directories(dest_dir);

    fs::path dst = dest_dir / src.filename();
    if (fs::exists(dst)) {
      const auto stem = dst.stem().string();
      const auto ext = dst.extension().string();
      dst = dest_dir / (stem + "_" + now_stamp_YYYYMMDD_HHMMSS() + ext);
    }

    fs::rename(src, dst);
    return true;
  } catch (const std::exception& e) {
    if (err) {
      *err = "Move failed for " + src.string() + ": " + e.what();
    }
    return false;
  }
}

void write_import_report(const std::string& report_path,
                         const ImportRunResult& result) {
  if (report_path.empty()) {
    return;
  }

  std::ofstream out(report_path, std::ios::trunc);
  if (!out) {
    return;
  }

  out << "Import report\n";
  out << "=============\n\n";
  out << "Files seen: " << result.files_seen << "\n";
  out << "Generals imported/updated: " << result.generals_imported << "\n";
  out << "Occurrences inserted: " << result.occurrences_inserted << "\n";
  out << "Occurrence insert failures: " << result.occurrence_insert_failures << "\n";
  out << "Pending examples inserted: " << result.pending_examples_inserted << "\n";
  out << "Files moved to imported: " << result.imported_files << "\n";
  out << "Files moved to invalid: " << result.invalid_files << "\n";
  out << "Files left untouched: " << result.untouched_files << "\n\n";
  if (!result.messages.empty()) {
    out << "Messages\n";
    out << "--------\n";
    for (const auto& msg : result.messages) {
      out << "- " << msg << "\n";
    }
  }
}

std::vector<fs::path> snapshot_import_files(const fs::path& import_dir,
                                            std::string* err) {
  std::vector<fs::path> files;
  try {
    for (const auto& ent : fs::directory_iterator(import_dir)) {
      if (!ent.is_regular_file()) {
        continue;
      }
      const auto p = ent.path();
      if (p.extension() != ".txt" || is_in_bak_dir(p) || looks_like_backup_file(p)) {
        continue;
      }
      files.push_back(p);
    }
  } catch (const std::exception& e) {
    if (err) {
      *err = e.what();
    }
    return {};
  }
  std::sort(files.begin(), files.end());
  return files;
}

std::optional<std::array<int, 5>> split_specialty_total_abs(int abs_total) {
  switch (abs_total) {
  case 6:
    return std::array<int, 5>{1, 1, 1, 1, 2};
  case 10:
    return std::array<int, 5>{1, 1, 2, 2, 4};
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
  case 35:
    return std::array<int, 5>{3, 4, 6, 7, 15};
  case 36:
    return std::array<int, 5>{4, 4, 7, 7, 14};
  case 40:
    return std::array<int, 5>{4, 4, 8, 8, 16};
  case 45:
    return std::array<int, 5>{4, 5, 9, 9, 18};
  case 46:
    return std::array<int, 5>{5, 5, 9, 9, 18};
  case 50:
    return std::array<int, 5>{5, 5, 10, 10, 20};
  case 55:
    return std::array<int, 5>{5, 6, 11, 11, 22};
  case 60:
    return std::array<int, 5>{6, 6, 12, 12, 24};
  case 90:
    return std::array<int, 5>{9, 9, 18, 18, 36};
  case 100:
    return std::array<int, 5>{10, 10, 20, 20, 40};
  case 200:
    return std::array<int, 5>{20, 20, 40, 40, 80};
  case 8:
    return std::array<int, 5>{1, 1, 2, 2, 2};
  case 5:
    return std::array<int, 5>{1, 1, 1, 1, 1};
  default:
    return std::nullopt;
  }
}

bool is_intish(double v) { return std::fabs(v - std::round(v)) < 1e-9; }

int parse_first_int(const std::string& s) {
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

void promote_singleton_specialty_l5_to_total(LoadedGeneralV2& g) {
  struct Key {
    std::string raw_key;
    int spec_n = 0;
    bool operator==(const Key& o) const {
      return raw_key == o.raw_key && spec_n == o.spec_n;
    }
  };
  struct KeyHash {
    size_t operator()(const Key& k) const {
      size_t h1 = std::hash<std::string>{}(k.raw_key);
      size_t h2 = std::hash<int>{}(k.spec_n);
      return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
  };
  struct Agg {
    int count_non_total_levels = 0;
    bool has_level_1to4 = false;
    bool has_total = false;
    int idx_l5 = -1;
  };

  std::unordered_map<Key, Agg, KeyHash> agg;
  for (int i = 0; i < (int)g.occurrences.size(); ++i) {
    const auto& o = g.occurrences[i];
    if (o.context_type != "Specialty" || !o.level.has_value()) {
      continue;
    }

    const int spec_n = parse_first_int(o.context_name);
    if (spec_n <= 0) {
      continue;
    }

    Key k{o.raw_key, spec_n};
    auto& a = agg[k];

    if (o.is_total) {
      a.has_total = true;
      continue;
    }

    const int lv = *o.level;
    if (lv >= 1 && lv <= 4) {
      a.has_level_1to4 = true;
    }
    if (lv >= 1 && lv <= 5) {
      a.count_non_total_levels++;
      a.idx_l5 = (lv == 5) ? i : -1;
    }
  }

  for (const auto& [k, a] : agg) {
    if (a.has_total || a.has_level_1to4) {
      continue;
    }
    if (a.count_non_total_levels == 1 && a.idx_l5 >= 0) {
      auto& o = g.occurrences[a.idx_l5];
      o.is_total = true;
      o.context_name = "SPECIALTY " + std::to_string(k.spec_n);
      o.raw_line = "PROMOTED_SINGLETON_L5_TO_TOTAL (Max Level Attributes file)";
    }
  }
}

void synthesize_specialty_levels_if_needed(LoadedGeneralV2& g) {
  struct Key {
    std::string raw_key;
    std::string context_type;
    std::string context_name;
    bool operator==(const Key& o) const {
      return raw_key == o.raw_key && context_type == o.context_type &&
             context_name == o.context_name;
    }
  };
  struct KeyHash {
    size_t operator()(const Key& k) const {
      std::hash<std::string> h;
      size_t x = h(k.raw_key);
      x ^= (h(k.context_type) << 1);
      x ^= (h(k.context_name) << 2);
      return x;
    }
  };

  std::unordered_map<Key, bool, KeyHash> has_any_level_1to4;
  std::unordered_map<Key, bool, KeyHash> has_level5_increment;

  for (const auto& o : g.occurrences) {
    if (o.context_type != "Specialty" || !o.level.has_value()) {
      continue;
    }
    Key k{o.raw_key, o.context_type, o.context_name};
    if (*o.level >= 1 && *o.level <= 4) {
      has_any_level_1to4[k] = true;
    }
    if (*o.level == 5 && !o.is_total) {
      has_level5_increment[k] = true;
    }
  }

  std::vector<StatOccurrenceV2> synth;

  for (const auto& o : g.occurrences) {
    if (o.context_type != "Specialty" || !o.level.has_value() ||
        *o.level != 5 || !o.is_total || !is_intish(o.value)) {
      continue;
    }

    const int abs_total = (int)std::llround(std::fabs(o.value));
    const auto split = split_specialty_total_abs(abs_total);
    if (!split.has_value()) {
      continue;
    }

    Key k{o.raw_key, o.context_type, o.context_name};
    if (has_any_level_1to4[k] || has_level5_increment[k]) {
      continue;
    }

    const double sign = (o.value < 0) ? -1.0 : 1.0;

    for (int lvl = 1; lvl <= 4; ++lvl) {
      StatOccurrenceV2 o2 = o;
      o2.level = lvl;
      o2.is_total = false;
      o2.value = sign * (double)(*split)[lvl - 1];
      o2.raw_line = "SYNTH_SPECIALTY_L" + std::to_string(lvl) +
                    " from TOTAL " + std::to_string(abs_total);
      synth.push_back(std::move(o2));
    }

    StatOccurrenceV2 o2 = o;
    o2.level = 5;
    o2.is_total = false;
    o2.value = sign * (double)(*split)[4];
    o2.raw_line = "SYNTH_SPECIALTY_L5_INCREMENT from TOTAL " +
                  std::to_string(abs_total);
    synth.push_back(std::move(o2));
  }

  if (!synth.empty()) {
    g.occurrences.insert(g.occurrences.end(), synth.begin(), synth.end());
  }
}

void synthesize_specialty_totals_if_missing(LoadedGeneralV2& g) {
  struct Key {
    std::string raw_key;
    int spec_n = 0;
    bool operator==(const Key& o) const {
      return raw_key == o.raw_key && spec_n == o.spec_n;
    }
  };
  struct KeyHash {
    size_t operator()(const Key& k) const {
      size_t x = std::hash<std::string>{}(k.raw_key);
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
  for (const auto& o : g.occurrences) {
    if (o.context_type != "Specialty" || !o.level.has_value()) {
      continue;
    }
    const int spec_n = parse_first_int(o.context_name);
    if (spec_n <= 0) {
      continue;
    }

    Key k{o.raw_key, spec_n};
    auto& a = agg[k];
    if (a.file_path.empty()) {
      a.file_path = o.file_path;
      a.line_number = o.line_number;
    }

    if (o.is_total) {
      a.has_total = true;
      continue;
    }

    const int lv = *o.level;
    if (lv >= 1 && lv <= 4) {
      a.has_level_1to4 = true;
    }
    if (lv >= 1 && lv <= 5) {
      a.sum += o.value;
      a.count++;
    }
  }

  std::vector<StatOccurrenceV2> synth;
  for (const auto& [k, a] : agg) {
    if (a.has_total || a.count <= 0) {
      continue;
    }
    if (!a.has_level_1to4 && a.count == 1) {
      continue;
    }

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

int preprocess_loaded_general(LoadedGeneralV2& g) {
  const int before = (int)g.occurrences.size();
  promote_singleton_specialty_l5_to_total(g);
  synthesize_specialty_levels_if_needed(g);
  synthesize_specialty_totals_if_missing(g);
  return (int)g.occurrences.size() - before;
}

struct PreviewDb {
  sqlite3* db = nullptr;
  ~PreviewDb() {
    if (db) {
      sqlite3_close(db);
    }
  }
};

bool open_preview_db(const std::string& path, PreviewDb& out, std::string* err) {
  if (sqlite3_open_v2(path.c_str(), &out.db, SQLITE_OPEN_READONLY, nullptr) !=
      SQLITE_OK) {
    if (err) {
      *err = out.db ? sqlite3_errmsg(out.db) : "sqlite3_open_v2 failed";
    }
    return false;
  }
  return true;
}

std::optional<int> preview_resolve_stat_key_id(sqlite3* db, const std::string& raw_key) {
  {
    const char* sql =
        "SELECT a.stat_key_id "
        "FROM stat_key_aliases a "
        "JOIN stat_keys k ON k.id = a.stat_key_id "
        "WHERE a.alias_key=?1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, raw_key.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return id;
      }
      sqlite3_finalize(stmt);
    }
  }

  {
    const char* sql = "SELECT id FROM stat_keys WHERE key=?1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, raw_key.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return id;
      }
      sqlite3_finalize(stmt);
    }
  }

  return std::nullopt;
}

void preview_lookup_general(sqlite3* db, const std::string& name, bool& exists,
                            bool& locked) {
  exists = false;
  locked = false;
  const char* sql = "SELECT double_checked_in_game FROM generals WHERE name=?1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    exists = true;
    locked = sqlite3_column_int(stmt, 0) == 1;
  }
  sqlite3_finalize(stmt);
}

}  // namespace

ImportPreviewResult preview_import_v2(const std::string& db_path,
                                      const std::string& import_path) {
  ImportPreviewResult result;
  const fs::path import_dir = fs::path(import_path);

  PreviewDb preview_db;
  std::string err;
  if (!open_preview_db(db_path, preview_db, &err)) {
    add_message(result, "Failed to open DB for preview: " + err);
    return result;
  }

  const std::vector<fs::path> files = snapshot_import_files(import_dir, &err);
  if (!err.empty()) {
    add_message(result, "Directory scan error: " + err);
    return result;
  }

  result.files_seen = (int)files.size();

  for (const auto& p : files) {
    ImportPreviewFile file{};
    file.file_path = p.string();

    LoadedGeneralV2 g = load_general_v2_from_file(p.string());
    if (!g.errors.empty()) {
      file.parse_ok = false;
      for (const auto& e : g.errors) {
        add_message(file, e);
      }
      result.files_parse_errors++;
      result.files.push_back(std::move(file));
      continue;
    }

    file.parse_ok = true;
    file.general_name = g.meta.name;
    preview_lookup_general(preview_db.db, g.meta.name, file.general_exists,
                           file.general_locked);
    if (file.general_exists) {
      result.generals_existing++;
    } else {
      result.generals_new++;
    }
    if (file.general_locked) {
      result.generals_locked++;
      add_message(file, "General is locked; importer will skip updates.");
    }

    file.synthesized_occurrences = preprocess_loaded_general(g);
    file.occurrence_count = (int)g.occurrences.size();
    result.synthesized_occurrences += file.synthesized_occurrences;
    result.occurrences_total += file.occurrence_count;

    for (const auto& o : g.occurrences) {
      if (preview_resolve_stat_key_id(preview_db.db, o.raw_key).has_value()) {
        file.resolved_occurrences++;
        result.resolved_occurrences++;
      } else {
        file.pending_occurrences++;
        result.pending_occurrences++;
      }
    }

    if (file.pending_occurrences > 0) {
      add_message(file, "Has unmapped stats that would be added to pending keys.");
    }
    if (file.synthesized_occurrences > 0) {
      add_message(file, "Importer would synthesize specialty rows.");
    }

    result.files.push_back(std::move(file));
  }

  result.ok = true;
  return result;
}

ImportRunResult run_import_v2(const std::string& db_path,
                              const std::string& import_path,
                              const std::string& report_path) {
  ImportRunResult result;

  const fs::path import_dir = fs::path(import_path);
  const fs::path data_dir = import_dir.parent_path();
  const fs::path imported_dir = data_dir / "imported";
  const fs::path invalid_dir = data_dir / "_invalid";

  try {
    fs::create_directories(imported_dir);
    fs::create_directories(invalid_dir);
  } catch (const std::exception& e) {
    result.messages.push_back(std::string("Failed to prepare import directories: ") +
                              e.what());
    write_import_report(report_path, result);
    return result;
  }

  DbImportV2 db;
  if (!db.open(db_path)) {
    add_message(result, "Failed to open DB for import: " + db_path);
    write_import_report(report_path, result);
    return result;
  }

  std::string scan_err;
  const std::vector<fs::path> files = snapshot_import_files(import_dir, &scan_err);
  if (!scan_err.empty()) {
    add_message(result, std::string("Directory scan error: ") + scan_err);
    write_import_report(report_path, result);
    return result;
  }
  result.files_seen = (int)files.size();

  for (const auto& p : files) {
    LoadedGeneralV2 g = load_general_v2_from_file(p.string());
    if (!g.errors.empty()) {
      std::ostringstream oss;
      oss << "Parse errors in " << p.filename().string();
      for (const auto& e : g.errors) {
        oss << " | " << e;
      }
      add_message(result, oss.str());
      std::string move_err;
      safe_move(p, invalid_dir, &move_err);
      if (!move_err.empty()) {
        add_message(result, move_err);
        result.untouched_files++;
      } else {
        result.invalid_files++;
      }
      continue;
    }

    preprocess_loaded_general(g);

    if (!db.begin()) {
      add_message(result, "Failed to begin transaction for " + p.string());
      result.untouched_files++;
      continue;
    }

    int general_id = -1;
    if (!db.upsert_general(g.meta.name, g.meta.role, g.meta.country,
                           g.meta.has_covenant, g.meta.covenant_member_1,
                           g.meta.covenant_member_2, g.meta.covenant_member_3,
                           (g.meta.role != "Unknown"), g.meta.in_tavern,
                           g.meta.base_skill_name, g.meta.leadership,
                           g.meta.leadership_green, g.meta.attack,
                           g.meta.attack_green, g.meta.defense,
                           g.meta.defense_green, g.meta.politics,
                           g.meta.politics_green, g.meta.source_text_verbatim,
                           g.meta.double_checked_in_game, general_id)) {
      db.rollback();
      std::string move_err;
      safe_move(p, invalid_dir, &move_err);
      if (!move_err.empty()) {
        add_message(result, move_err);
        result.untouched_files++;
      } else {
        result.invalid_files++;
      }
      add_message(result,
                  "Failed to upsert general from " + p.filename().string());
      continue;
    }

    db.delete_occurrences_for_general_file(general_id, p.string());

    for (const auto& o : g.occurrences) {
      auto stat_id = db.resolve_stat_key_id(o.raw_key);
      if (!stat_id) {
        DbImportV2::PendingInfo pi;
        if (db.ensure_pending_key(o.raw_key, o.file_path, o.line_number, pi)) {
          db.add_pending_example(pi.pending_id, g.meta.name, o.context_type,
                                 o.context_name, o.level, o.value, o.file_path,
                                 o.line_number, o.raw_line);
          result.pending_examples_inserted++;
        }
        continue;
      }

      if (db.insert_stat_occurrence(general_id, *stat_id, o.value, o.context_type,
                                    o.context_name, o.level, o.is_total,
                                    o.file_path, o.line_number, o.raw_line)) {
        result.occurrences_inserted++;
      } else {
        result.occurrence_insert_failures++;
        std::ostringstream oss;
        oss << "Occurrence insert failed"
            << " | general=" << g.meta.name
            << " | raw_key=" << o.raw_key
            << " | stat_key_id=" << *stat_id
            << " | file=" << o.file_path << ":" << o.line_number
            << " | context=" << o.context_type << "/" << o.context_name;
        add_message(result, oss.str());
      }
    }

    if (!db.commit()) {
      db.rollback();
      std::string move_err;
      safe_move(p, invalid_dir, &move_err);
      if (!move_err.empty()) {
        add_message(result, move_err);
        result.untouched_files++;
      } else {
        result.invalid_files++;
      }
      add_message(result,
                  "Failed to commit import for " + p.filename().string());
      continue;
    }

    result.generals_imported++;
    std::string move_err;
    safe_move(p, imported_dir, &move_err);
    if (!move_err.empty()) {
      add_message(result, move_err);
      result.untouched_files++;
    } else {
      result.imported_files++;
    }
  }

  result.ok = true;
  write_import_report(report_path, result);
  return result;
}

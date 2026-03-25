#include "analyzer/model.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace {

int parse_first_int(const std::string& s) {
  for (size_t i = 0; i < s.size(); ++i) {
    if (std::isdigit(static_cast<unsigned char>(s[i]))) {
      int value = 0;
      while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        value = value * 10 + (s[i] - '0');
        ++i;
      }
      return value;
    }
  }
  return 0;
}

void add_value(std::map<std::string, double>& dest, const std::string& key,
               double value) {
  dest[key] += value;
}

}  // namespace

std::vector<AnalyzerGeneralListItem> analyzer_load_general_list(AnalyzerDb& db) {
  auto stmt = db.prepare(
      "SELECT g.id, g.name, g.role, g.has_covenant, "
      "COALESCE((SELECT MAX(CAST(TRIM(SUBSTR(s.context_name, 10)) AS INT)) "
      "FROM stat_occurrences s "
      "WHERE s.general_id = g.id "
      "  AND s.context_type='Covenant' "
      "  AND (s.context_name LIKE 'COVENANT %' OR s.context_name LIKE 'Covenant "
      "%')), 0) AS covenant_max, "
      "g.in_tavern "
      "FROM generals g "
      "WHERE g.double_checked_in_game = 1 "
      "ORDER BY g.name;");

  std::vector<AnalyzerGeneralListItem> out;
  while (stmt.step_row()) {
    AnalyzerGeneralListItem item;
    item.id = stmt.col_int(0);
    item.name = stmt.col_text(1);
    item.role = stmt.col_text(2);
    item.has_covenant = stmt.col_int(3);
    item.covenant_max = stmt.col_int(4);
    item.in_tavern = stmt.col_int(5);
    out.push_back(std::move(item));
  }
  return out;
}

std::vector<std::string> analyzer_load_canonical_stat_keys(AnalyzerDb& db) {
  auto stmt = db.prepare(
      "SELECT key "
      "FROM stat_keys "
      "WHERE is_active = 1 "
      "ORDER BY key;");

  std::vector<std::string> out;
  while (stmt.step_row()) {
    out.push_back(stmt.col_text(0));
  }
  return out;
}

AnalyzerGeneralData analyzer_load_general_data(AnalyzerDb& db, int general_id) {
  AnalyzerGeneralData data;
  std::map<std::tuple<int, std::string>, double> specialty_total_only_rows;
  std::map<std::tuple<int, std::string>, int> specialty_split_counts;

  {
    auto stmt = db.prepare(
        "SELECT id, name, role, has_covenant "
        "FROM generals "
        "WHERE id=?1 AND double_checked_in_game=1;");
    stmt.bind_int(1, general_id);
    if (!stmt.step_row()) {
      throw std::runtime_error("checked general not found");
    }
    data.id = stmt.col_int(0);
    data.name = stmt.col_text(1);
    data.role = stmt.col_text(2);
    data.has_covenant = stmt.col_int(3);
  }

  auto stmt = db.prepare(
      "SELECT s.context_type, s.context_name, s.level, s.is_total, k.key, "
      "s.value "
      "FROM stat_occurrences s "
      "JOIN stat_keys k ON k.id = s.stat_key_id "
      "WHERE s.general_id = ?1 "
      "ORDER BY s.context_type, s.context_name, s.level, s.is_total, k.key;");
  stmt.bind_int(1, general_id);

  while (stmt.step_row()) {
    const std::string context_type = stmt.col_text(0);
    const std::string context_name = stmt.col_text(1);
    const int level = stmt.col_is_null(2) ? 0 : stmt.col_int(2);
    const int is_total = stmt.col_int(3);
    const std::string stat_key = stmt.col_text(4);
    const double value = stmt.col_double(5);

    if (context_type == "BaseSkill") {
      add_value(data.base_stats, stat_key, value);
      continue;
    }

    if (context_type == "Ascension") {
      const int ascension_idx = parse_first_int(context_name);
      if (ascension_idx >= 1 && ascension_idx <= 5) {
        add_value(data.ascension_stats[(size_t)ascension_idx], stat_key, value);
      }
      continue;
    }

    if (context_type == "Specialty") {
      const int specialty_idx = parse_first_int(context_name);
      if (specialty_idx < 1 || specialty_idx > 4) {
        std::ostringstream oss;
        oss << "Skipped specialty row with unexpected shape: " << context_name
            << " level=" << level;
        data.warnings.push_back(oss.str());
        continue;
      }

      auto key = std::make_tuple(specialty_idx, stat_key);
      if (is_total) {
        specialty_total_only_rows[key] = value;
        continue;
      }

      if (level < 1 || level > 5) {
        std::ostringstream oss;
        oss << "Skipped specialty row with unexpected shape: " << context_name
            << " level=" << level;
        data.warnings.push_back(oss.str());
        continue;
      }

      specialty_split_counts[key] += 1;
      add_value(data.specialty_stats[(size_t)(specialty_idx - 1)][(size_t)level],
                stat_key, value);
      continue;
    }

    if (context_type == "Covenant") {
      const int covenant_idx = parse_first_int(context_name);
      if (covenant_idx >= 1 && covenant_idx <= 6) {
        add_value(data.covenant_stats[(size_t)covenant_idx], stat_key, value);
        data.covenant_max = std::max(data.covenant_max, covenant_idx);
      }
      continue;
    }
  }

  if (data.has_covenant == 0) {
    data.covenant_max = 0;
  }

  for (const auto& [key, total_value] : specialty_total_only_rows) {
    if (specialty_split_counts[key] > 0) {
      continue;
    }

    const int specialty_idx = std::get<0>(key);
    const std::string& stat_key = std::get<1>(key);
    add_value(data.specialty_stats[(size_t)(specialty_idx - 1)][5], stat_key,
              total_value);
  }

  return data;
}

#include "db_admin.h"

#include <fstream>
#include <sstream>
#include <string>

namespace {

int query_count(sqlite3* db, const char* sql, std::string* err) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    if (err) {
      *err = sqlite3_errmsg(db);
    }
    return -1;
  }

  int out = -1;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    out = sqlite3_column_int(stmt, 0);
  } else if (err) {
    *err = sqlite3_errmsg(db);
  }
  sqlite3_finalize(stmt);
  return out;
}

void add_group_messages(sqlite3* db, const char* sql, std::vector<std::string>& out) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    out.push_back(std::string("Failed to prepare integrity detail query: ") +
                  sqlite3_errmsg(db));
    return;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char* txt = sqlite3_column_text(stmt, 0);
    out.push_back(txt ? reinterpret_cast<const char*>(txt) : "");
  }
  sqlite3_finalize(stmt);
}

bool exec_sql(sqlite3* db, const char* sql, std::string* err) {
  char* raw_err = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &raw_err) != SQLITE_OK) {
    if (err) {
      *err = raw_err ? raw_err : "unknown";
    }
    sqlite3_free(raw_err);
    return false;
  }
  return true;
}

void collect_repair_items(sqlite3* db, const char* sql,
                          const std::string& category,
                          std::vector<IntegrityRepairItem>& out) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    out.push_back(
        {category,
         std::string("Failed to prepare repair-item query: ") + sqlite3_errmsg(db),
         0,
         "",
         false});
    return;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    IntegrityRepairItem item;
    item.category = category;
    const unsigned char* label = sqlite3_column_text(stmt, 0);
    item.label = label ? reinterpret_cast<const char*>(label) : "";
    item.row_count = sqlite3_column_int(stmt, 1);
    const unsigned char* delete_sql = sqlite3_column_text(stmt, 2);
    item.delete_sql = delete_sql ? reinterpret_cast<const char*>(delete_sql) : "";
    item.selected = item.row_count > 0 && !item.delete_sql.empty();
    out.push_back(std::move(item));
  }
  sqlite3_finalize(stmt);
}

}  // namespace

IntegritySummary audit_db_integrity(sqlite3* db) {
  IntegritySummary summary;
  if (!db) {
    summary.messages.push_back("No open database.");
    return summary;
  }

  std::string err;
  summary.orphan_stat_occurrences_general = query_count(
      db,
      "SELECT COUNT(*) FROM stat_occurrences "
      "WHERE general_id NOT IN (SELECT id FROM generals);",
      &err);
  if (summary.orphan_stat_occurrences_general < 0) {
    summary.messages.push_back("Failed to count orphan stat_occurrences by general: " + err);
    return summary;
  }

  summary.orphan_stat_occurrences_stat_key = query_count(
      db,
      "SELECT COUNT(*) FROM stat_occurrences "
      "WHERE stat_key_id NOT IN (SELECT id FROM stat_keys);",
      &err);
  if (summary.orphan_stat_occurrences_stat_key < 0) {
    summary.messages.push_back("Failed to count orphan stat_occurrences by stat_key: " + err);
    return summary;
  }

  summary.orphan_pending_examples = query_count(
      db,
      "SELECT COUNT(*) FROM pending_stat_key_examples "
      "WHERE pending_id NOT IN (SELECT id FROM pending_stat_keys);",
      &err);
  if (summary.orphan_pending_examples < 0) {
    summary.messages.push_back("Failed to count orphan pending examples: " + err);
    return summary;
  }

  summary.orphan_aliases = query_count(
      db,
      "SELECT COUNT(*) FROM stat_key_aliases "
      "WHERE stat_key_id NOT IN (SELECT id FROM stat_keys);",
      &err);
  if (summary.orphan_aliases < 0) {
    summary.messages.push_back("Failed to count orphan aliases: " + err);
    return summary;
  }

  summary.orphan_transforms_alias = query_count(
      db,
      "SELECT COUNT(*) FROM stat_key_transforms "
      "WHERE alias_key NOT IN (SELECT alias_key FROM stat_key_aliases);",
      &err);
  if (summary.orphan_transforms_alias < 0) {
    summary.messages.push_back("Failed to count orphan transforms by alias: " + err);
    return summary;
  }

  summary.orphan_transforms_stat_key = query_count(
      db,
      "SELECT COUNT(*) FROM stat_key_transforms "
      "WHERE stat_key_id NOT IN (SELECT id FROM stat_keys);",
      &err);
  if (summary.orphan_transforms_stat_key < 0) {
    summary.messages.push_back("Failed to count orphan transforms by stat_key: " + err);
    return summary;
  }

  add_group_messages(
      db,
      "SELECT "
      "  'Missing general id=' || so.general_id || "
      "  ' count=' || COUNT(*) || "
      "  ' sample_file=' || MIN(so.file_path) "
      "FROM stat_occurrences so "
      "LEFT JOIN generals g ON g.id = so.general_id "
      "WHERE g.id IS NULL "
      "GROUP BY so.general_id "
      "ORDER BY COUNT(*) DESC "
      "LIMIT 10;",
      summary.messages);

  add_group_messages(
      db,
      "SELECT "
      "  'Orphan pending_id=' || pending_id || "
      "  ' count=' || COUNT(*) || "
      "  ' sample_general=' || MIN(general_name) || "
      "  ' sample_file=' || MIN(file_path) "
      "FROM pending_stat_key_examples "
      "WHERE pending_id NOT IN (SELECT id FROM pending_stat_keys) "
      "GROUP BY pending_id "
      "ORDER BY COUNT(*) DESC "
      "LIMIT 10;",
      summary.messages);

  add_group_messages(
      db,
      "SELECT "
      "  'Orphan alias ' || alias_key || ' -> stat_key_id=' || stat_key_id "
      "FROM stat_key_aliases "
      "WHERE stat_key_id NOT IN (SELECT id FROM stat_keys) "
      "ORDER BY alias_key "
      "LIMIT 20;",
      summary.messages);

  summary.repair_items.clear();
  collect_repair_items(
      db,
      "SELECT "
      "  'Delete orphan stat_occurrences for missing general_id=' || so.general_id || "
      "  ' sample_file=' || MIN(so.file_path), "
      "  COUNT(*), "
      "  'DELETE FROM stat_occurrences WHERE general_id=' || so.general_id || "
      "  ' AND general_id NOT IN (SELECT id FROM generals);' "
      "FROM stat_occurrences so "
      "LEFT JOIN generals g ON g.id = so.general_id "
      "WHERE g.id IS NULL "
      "GROUP BY so.general_id "
      "ORDER BY COUNT(*) DESC;",
      "stat_occurrences.missing_general",
      summary.repair_items);
  collect_repair_items(
      db,
      "SELECT "
      "  'Delete orphan stat_occurrences for missing stat_key_id=' || so.stat_key_id || "
      "  ' sample_file=' || MIN(so.file_path), "
      "  COUNT(*), "
      "  'DELETE FROM stat_occurrences WHERE stat_key_id=' || so.stat_key_id || "
      "  ' AND stat_key_id NOT IN (SELECT id FROM stat_keys);' "
      "FROM stat_occurrences so "
      "LEFT JOIN stat_keys sk ON sk.id = so.stat_key_id "
      "WHERE sk.id IS NULL "
      "GROUP BY so.stat_key_id "
      "ORDER BY COUNT(*) DESC;",
      "stat_occurrences.missing_stat_key",
      summary.repair_items);
  collect_repair_items(
      db,
      "SELECT "
      "  'Delete orphan pending examples for pending_id=' || pe.pending_id || "
      "  ' sample_general=' || MIN(pe.general_name) || "
      "  ' sample_file=' || MIN(pe.file_path), "
      "  COUNT(*), "
      "  'DELETE FROM pending_stat_key_examples WHERE pending_id=' || pe.pending_id || "
      "  ' AND pending_id NOT IN (SELECT id FROM pending_stat_keys);' "
      "FROM pending_stat_key_examples pe "
      "LEFT JOIN pending_stat_keys pk ON pk.id = pe.pending_id "
      "WHERE pk.id IS NULL "
      "GROUP BY pe.pending_id "
      "ORDER BY COUNT(*) DESC;",
      "pending_examples.missing_pending_key",
      summary.repair_items);
  collect_repair_items(
      db,
      "SELECT "
      "  'Delete orphan alias ' || alias_key || ' -> stat_key_id=' || stat_key_id, "
      "  1, "
      "  'DELETE FROM stat_key_aliases WHERE alias_key=' || quote(alias_key) || ';' "
      "FROM stat_key_aliases "
      "WHERE stat_key_id NOT IN (SELECT id FROM stat_keys) "
      "ORDER BY alias_key;",
      "stat_key_aliases.missing_stat_key",
      summary.repair_items);
  collect_repair_items(
      db,
      "SELECT "
      "  'Delete orphan transforms for missing alias_key=' || alias_key, "
      "  COUNT(*), "
      "  'DELETE FROM stat_key_transforms WHERE alias_key=' || quote(alias_key) || "
      "  ' AND alias_key NOT IN (SELECT alias_key FROM stat_key_aliases);' "
      "FROM stat_key_transforms "
      "WHERE alias_key NOT IN (SELECT alias_key FROM stat_key_aliases) "
      "GROUP BY alias_key "
      "ORDER BY COUNT(*) DESC;",
      "stat_key_transforms.missing_alias",
      summary.repair_items);
  collect_repair_items(
      db,
      "SELECT "
      "  'Delete orphan transforms for missing stat_key_id=' || stat_key_id, "
      "  COUNT(*), "
      "  'DELETE FROM stat_key_transforms WHERE stat_key_id=' || stat_key_id || "
      "  ' AND stat_key_id NOT IN (SELECT id FROM stat_keys);' "
      "FROM stat_key_transforms "
      "WHERE stat_key_id NOT IN (SELECT id FROM stat_keys) "
      "GROUP BY stat_key_id "
      "ORDER BY COUNT(*) DESC;",
      "stat_key_transforms.missing_stat_key",
      summary.repair_items);

  summary.ok = true;
  return summary;
}

bool repair_db_integrity(sqlite3* db,
                         const std::vector<IntegrityRepairItem>& items,
                         const std::string& report_path,
                         std::string* err) {
  if (!db) {
    if (err) {
      *err = "No open database.";
    }
    return false;
  }

  if (!exec_sql(db, "BEGIN;", err)) {
    return false;
  }

  std::ostringstream report;
  report << "Integrity repair report\n";
  report << "=======================\n\n";
  int selected_items = 0;
  int total_deleted = 0;

  for (const auto& item : items) {
    if (!item.selected || item.delete_sql.empty()) {
      continue;
    }
    selected_items++;
    if (!exec_sql(db, item.delete_sql.c_str(), err)) {
      std::string rollback_err;
      exec_sql(db, "ROLLBACK;", &rollback_err);
      return false;
    }
    const int deleted = sqlite3_changes(db);
    total_deleted += deleted;
    report << item.category << "\n";
    report << "  Label: " << item.label << "\n";
    report << "  Preview row count: " << item.row_count << "\n";
    report << "  Deleted rows: " << deleted << "\n";
    report << "  SQL: " << item.delete_sql << "\n\n";
  }

  if (!exec_sql(db, "COMMIT;", err)) {
    std::string rollback_err;
    exec_sql(db, "ROLLBACK;", &rollback_err);
    return false;
  }

  report << "Summary\n";
  report << "-------\n";
  report << "Selected repair items: " << selected_items << "\n";
  report << "Total deleted rows: " << total_deleted << "\n";

  if (!report_path.empty()) {
    std::ofstream out(report_path, std::ios::trunc);
    if (out) {
      out << report.str();
    } else if (err) {
      *err = "Repair completed, but failed to write report: " + report_path;
    }
  }

  return true;
}

bool promote_unchecked_singleton_specialty_l5_to_total(sqlite3* db,
                                                        int* changed_rows,
                                                        std::string* err) {
  if (!db) {
    if (err) {
      *err = "No open database.";
    }
    return false;
  }

  if (!exec_sql(db, "BEGIN;", err)) {
    return false;
  }

  const char* sql =
      "WITH candidate_rows AS ("
      "  SELECT o.id AS occurrence_id, "
      "         CAST(SUBSTR(o.context_name, 11) AS INT) AS spec_n "
      "  FROM stat_occurrences o "
      "  JOIN generals g ON g.id = o.general_id "
      "  JOIN ("
      "    SELECT general_id, "
      "           CAST(SUBSTR(context_name, 11) AS INT) AS spec_n, "
      "           stat_key_id "
      "    FROM stat_occurrences "
      "    WHERE context_type='Specialty' "
      "    GROUP BY general_id, spec_n, stat_key_id "
      "    HAVING COUNT(*) = 1 "
      "       AND SUM(CASE WHEN is_total = 1 THEN 1 ELSE 0 END) = 0 "
      "       AND SUM(CASE WHEN level BETWEEN 1 AND 4 THEN 1 ELSE 0 END) = 0 "
      "       AND SUM(CASE WHEN level = 5 AND is_total = 0 THEN 1 ELSE 0 END) = 1"
      "  ) cand "
      "    ON cand.general_id = o.general_id "
      "   AND cand.spec_n = CAST(SUBSTR(o.context_name, 11) AS INT) "
      "   AND cand.stat_key_id = o.stat_key_id "
      "  WHERE g.double_checked_in_game = 0 "
      "    AND o.context_type = 'Specialty' "
      "    AND o.level = 5 "
      "    AND o.is_total = 0"
      ") "
      "UPDATE stat_occurrences "
      "SET is_total = 1, "
      "    context_name = 'SPECIALTY ' || "
      "      (SELECT spec_n FROM candidate_rows WHERE occurrence_id = stat_occurrences.id) || "
      "      ' L5 (TOTAL)', "
      "    raw_line = 'PROMOTED_SINGLETON_L5_TO_TOTAL (Admin)' "
      "WHERE id IN (SELECT occurrence_id FROM candidate_rows);";

  if (!exec_sql(db, sql, err)) {
    std::string rollback_err;
    exec_sql(db, "ROLLBACK;", &rollback_err);
    return false;
  }

  const int changed = sqlite3_changes(db);

  if (!exec_sql(db, "COMMIT;", err)) {
    std::string rollback_err;
    exec_sql(db, "ROLLBACK;", &rollback_err);
    return false;
  }

  if (changed_rows) {
    *changed_rows = changed;
  }
  return true;
}

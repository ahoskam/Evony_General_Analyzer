#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>

struct IntegrityRepairItem {
  std::string category;
  std::string label;
  int row_count = 0;
  std::string delete_sql;
  bool selected = true;
};

struct IntegritySummary {
  bool ok = false;
  int orphan_stat_occurrences_general = 0;
  int orphan_stat_occurrences_stat_key = 0;
  int orphan_pending_examples = 0;
  int orphan_aliases = 0;
  int orphan_transforms_alias = 0;
  int orphan_transforms_stat_key = 0;
  std::vector<std::string> messages;
  std::vector<IntegrityRepairItem> repair_items;
};

IntegritySummary audit_db_integrity(sqlite3* db);
bool repair_db_integrity(sqlite3* db,
                         const std::vector<IntegrityRepairItem>& items,
                         const std::string& report_path,
                         std::string* err = nullptr);
bool promote_unchecked_singleton_specialty_l5_to_total(sqlite3* db,
                                                        int* changed_rows = nullptr,
                                                        std::string* err = nullptr);

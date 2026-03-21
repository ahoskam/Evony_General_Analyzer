#pragma once

#include <optional>
#include <string>
#include <vector>

struct ImportPreviewFile {
  std::string file_path;
  std::string general_name;
  bool parse_ok = false;
  bool general_exists = false;
  bool general_locked = false;
  int occurrence_count = 0;
  int resolved_occurrences = 0;
  int pending_occurrences = 0;
  int synthesized_occurrences = 0;
  std::vector<std::string> messages;
};

struct ImportPreviewResult {
  bool ok = false;
  int files_seen = 0;
  int files_parse_errors = 0;
  int generals_new = 0;
  int generals_existing = 0;
  int generals_locked = 0;
  int occurrences_total = 0;
  int resolved_occurrences = 0;
  int pending_occurrences = 0;
  int synthesized_occurrences = 0;
  std::vector<ImportPreviewFile> files;
  std::vector<std::string> messages;
};

struct ImportRunResult {
  bool ok = false;
  int files_seen = 0;
  int generals_imported = 0;
  int occurrences_inserted = 0;
  int occurrence_insert_failures = 0;
  int pending_examples_inserted = 0;
  int invalid_files = 0;
  int imported_files = 0;
  int untouched_files = 0;
  std::vector<std::string> messages;
};

ImportPreviewResult preview_import_v2(const std::string& db_path,
                                      const std::string& import_path);

ImportRunResult run_import_v2(const std::string& db_path,
                              const std::string& import_path,
                              const std::string& report_path = "");

#pragma once
#include "db_admin.h"
#include "importer/import_service.h"
#include "model.h"
#include <vector>
#include <string>

struct EditorState {
  // selection / list filters
  std::string filter_name;
  std::string filter_role = "All";

  // loaded list
  std::vector<GeneralRow> list;
  std::vector<std::string> all_general_names;

  // selection
  int selected_general_id = 0;
  int pending_select_general_id = 0; // for dirty prompt flow

  // edit buffer
  GeneralMeta meta;
  std::vector<Occurrence> occ;

  // pending buffer (read-only from DB, but user can convert)
  std::vector<PendingExample> pending;

  // stat keys cache
  std::vector<StatKey> stat_keys;

  // pending UI: chosen mapping per pending row
  // index aligned to st.pending
  std::vector<int> pending_chosen_stat_key_id;

  // dirty tracking
  bool dirty = false;
  std::vector<int> deleted_occurrence_ids;

  // modal state
  bool show_dirty_modal = false;
  bool show_unmapped_delete_modal = false;
  int pending_unmapped_delete_pending_id = 0;
  std::string pending_unmapped_delete_context_type;
  int pending_unmapped_delete_context_index = 0;
  std::string pending_unmapped_delete_raw_key;

  // UI mode
  // pretty_view = true: read-only, compact verification view
  // pretty_view = false: full edit view (current editor)
  bool pretty_view = true;

  // validation errors
  std::vector<std::string> save_block_errors;
   // last save failure message (shown in UI)
  std::string last_save_error;

  // import UI
  std::string import_path = "data/import";
  std::string import_report_path = "data/last_import_report.txt";
  std::string import_status;
  bool import_last_ok = true;
  bool show_import_modal = false;
  ImportPreviewResult import_preview;
  bool has_import_preview = false;

  // admin / integrity UI
  bool show_integrity_modal = false;
  IntegritySummary integrity_summary;
  bool has_integrity_summary = false;
  std::string integrity_status;
  std::string integrity_report_path = "data/last_integrity_repair.txt";
};

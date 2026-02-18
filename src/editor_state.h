#pragma once
#include "model.h"
#include <vector>
#include <string>

struct EditorState {
  // selection / list filters
  std::string filter_name;
  std::string filter_role = "All";

  // loaded list
  std::vector<GeneralRow> list;

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

  // validation errors
  std::vector<std::string> save_block_errors;
};

#pragma once

#include "analyzer/json.h"
#include "analyzer/model.h"
#include "analyzer/readonly_db.h"

#include <string>
#include <vector>

struct AnalyzerAppState {
  std::vector<AnalyzerGeneralListItem> general_list;
  int selected_general_id = 0;
  bool has_loaded_selected = false;
  AnalyzerGeneralData selected_general;
  OwnedGeneralState selected_owned;
  bool selected_is_owned = false;
  OwnedStateFile owned_file;
  std::string state_path;
  std::string search_text;
  std::string status_message;
  bool dirty = false;
};

void analyzer_ui_tick(AnalyzerDb& db, AnalyzerAppState& state);

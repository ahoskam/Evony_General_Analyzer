#pragma once

#include "analyzer/json.h"
#include "analyzer/model.h"
#include "analyzer/readonly_db.h"

#include <map>
#include <string>
#include <vector>

enum class AnalyzerScoreMode {
  CanonicalStatKey = 0,
  TroopContext = 1,
};

enum class AnalyzerTavernFilter {
  Any = 0,
  InTavern = 1,
  NotInTavern = 2,
};

enum class AnalyzerOwnershipFilter {
  AllGenerals = 0,
  OwnedOnly = 1,
};

enum class AnalyzerTroopContext {
  Attacking = 0,
  Reinforcing = 1,
  InCity = 2,
};

enum class AnalyzerTroopStat {
  Attack = 0,
  Defense = 1,
  HP = 2,
};

struct AnalyzerRankedResult {
  int general_id = 0;
  std::string general_name;
  std::string role;
  bool in_tavern = false;
  bool is_owned = false;
  double total_score = 0.0;
  double specific_score = 0.0;
  double troop_type_score = 0.0;
  double generic_score = 0.0;
};

struct AnalyzerAppState {
  std::vector<AnalyzerGeneralListItem> general_list;
  std::vector<std::string> canonical_stat_keys;
  std::map<int, AnalyzerGeneralData> general_data_cache;
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
  int active_tab = 0;

  AnalyzerScoreMode score_mode = AnalyzerScoreMode::TroopContext;
  int canonical_stat_key_index = 0;
  std::string canonical_stat_key;
  std::string ranking_role_filter = "All";
  std::string troop_type = "Mounted";
  AnalyzerTroopContext troop_context = AnalyzerTroopContext::Reinforcing;
  AnalyzerTroopStat troop_stat = AnalyzerTroopStat::Defense;
  AnalyzerTavernFilter tavern_filter = AnalyzerTavernFilter::Any;
  AnalyzerOwnershipFilter ownership_filter = AnalyzerOwnershipFilter::AllGenerals;
  std::vector<AnalyzerRankedResult> ranked_results;
};

void analyzer_ui_tick(AnalyzerDb& db, AnalyzerAppState& state);

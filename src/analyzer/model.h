#pragma once

#include "analyzer/readonly_db.h"

#include <array>
#include <map>
#include <string>
#include <vector>

struct AnalyzerGeneralListItem {
  int id = 0;
  std::string name;
  std::string role;
  int has_covenant = 0;
  int covenant_max = 0;
};

struct AnalyzerGeneralData {
  int id = 0;
  std::string name;
  std::string role;
  int has_covenant = 0;
  int covenant_max = 0;

  std::map<std::string, double> base_stats;
  std::array<std::map<std::string, double>, 6> ascension_stats;
  std::array<std::array<std::map<std::string, double>, 6>, 4> specialty_stats;
  std::array<std::map<std::string, double>, 7> covenant_stats;

  std::vector<std::string> warnings;
};

std::vector<AnalyzerGeneralListItem> analyzer_load_general_list(AnalyzerDb& db);
AnalyzerGeneralData analyzer_load_general_data(AnalyzerDb& db, int general_id);

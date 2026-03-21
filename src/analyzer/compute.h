#pragma once

#include "analyzer/model.h"

#include <array>
#include <map>
#include <string>

struct OwnedGeneralState {
  int general_id = 0;
  std::string general_name;
  bool locked = false;
  bool has_dragon = false;
  bool has_spirit_beast = false;
  int general_level = 1;
  int ascension_level = 0;
  std::array<int, 4> specialty_levels{0, 0, 0, 0};
  int covenant_level = 0;

  std::string cached_input_key;
  std::map<std::string, double> cached_totals;
};

std::string owned_state_input_key(const AnalyzerGeneralData& data,
                                  const OwnedGeneralState& owned);
std::map<std::string, double> compute_total_stats(const AnalyzerGeneralData& data,
                                                  OwnedGeneralState& owned);
std::map<std::string, double> compute_assistant_total_stats(
    const AnalyzerGeneralData& data, const OwnedGeneralState& owned);

#include "analyzer/compute.h"

#include <algorithm>
#include <sstream>

namespace {

void merge_into(std::map<std::string, double>& dest,
                const std::map<std::string, double>& src) {
  for (const auto& [key, value] : src) {
    dest[key] += value;
  }
}

}  // namespace

std::string owned_state_input_key(const AnalyzerGeneralData& data,
                                  const OwnedGeneralState& owned) {
  std::ostringstream oss;
  oss << "gid=" << data.id << ";level=" << std::max(0, owned.general_level)
      << ";asc=" << std::clamp(owned.ascension_level, 0, 5) << ";spec="
      << std::clamp(owned.specialty_levels[0], 0, 5) << ","
      << std::clamp(owned.specialty_levels[1], 0, 5) << ","
      << std::clamp(owned.specialty_levels[2], 0, 5) << ","
      << std::clamp(owned.specialty_levels[3], 0, 5) << ";cov="
      << std::clamp(owned.covenant_level, 0, data.covenant_max);
  return oss.str();
}

std::map<std::string, double> compute_total_stats(const AnalyzerGeneralData& data,
                                                  OwnedGeneralState& owned) {
  const std::string key = owned_state_input_key(data, owned);
  if (owned.cached_input_key == key && !owned.cached_totals.empty()) {
    return owned.cached_totals;
  }

  std::map<std::string, double> totals = data.base_stats;

  const int ascension_level = std::clamp(owned.ascension_level, 0, 5);
  for (int level = 1; level <= ascension_level; ++level) {
    merge_into(totals, data.ascension_stats[(size_t)level]);
  }

  for (size_t spec = 0; spec < owned.specialty_levels.size(); ++spec) {
    const int specialty_level =
        std::clamp(owned.specialty_levels[spec], 0, 5);
    for (int level = 1; level <= specialty_level; ++level) {
      merge_into(totals, data.specialty_stats[spec][(size_t)level]);
    }
  }

  const int covenant_level = data.has_covenant
                                 ? std::clamp(owned.covenant_level, 0,
                                              data.covenant_max)
                                 : 0;
  for (int level = 1; level <= covenant_level; ++level) {
    merge_into(totals, data.covenant_stats[(size_t)level]);
  }

  owned.cached_input_key = key;
  owned.cached_totals = totals;
  return totals;
}

std::map<std::string, double> compute_assistant_total_stats(
    const AnalyzerGeneralData& data, const OwnedGeneralState& owned) {
  std::map<std::string, double> totals = data.base_stats;

  for (size_t spec = 0; spec < owned.specialty_levels.size(); ++spec) {
    const int specialty_level =
        std::clamp(owned.specialty_levels[spec], 0, 5);
    for (int level = 1; level <= specialty_level; ++level) {
      merge_into(totals, data.specialty_stats[spec][(size_t)level]);
    }
  }

  const int covenant_level = data.has_covenant
                                 ? std::clamp(owned.covenant_level, 0,
                                              data.covenant_max)
                                 : 0;
  for (int level = 1; level <= covenant_level; ++level) {
    merge_into(totals, data.covenant_stats[(size_t)level]);
  }

  return totals;
}

#pragma once
#include "Stat.h"
#include "Stats.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

std::optional<Stat> stat_from_key(const std::string& key);
std::string_view key_from_stat(Stat s);

ModifierKind kind_for_stat(Stat s);

// All keys your parser currently supports (derived from its own map)
const std::vector<std::string_view>& supported_stat_keys();

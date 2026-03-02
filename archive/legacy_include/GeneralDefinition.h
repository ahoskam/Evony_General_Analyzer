#pragma once
#include <string>
#include <vector>
#include <array>
#include <map>

#include "Stats.h"

struct GeneralDefinition {
    std::string name;
    std::string role; // "Ground", "Mounted", etc.

    // Always applied
    std::vector<Modifier> base;

    // Ascension level -> modifiers unlocked at that level
    std::map<int, std::vector<Modifier>> ascension;

    // 4 specialties, each with 5 levels
    // specialty[index][level] -> modifiers
    std::array<std::array<std::vector<Modifier>, 5>, 4> specialties{};

    // Covenants: 1..6 -> modifiers
    std::map<int, std::vector<Modifier>> covenants;

    // Optional covenant name for UI/debug
    std::map<int, std::string> covenantNames;
};

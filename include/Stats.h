#pragma once
#include <array>
#include <string>
#include <vector>
#include <string_view>
#include <optional>

#include "Stat.h"

enum class ModifierKind : int {
    Percent, // store as "76" meaning 76%
    Flat     // store as flat units (e.g., +50000 march size)
};

struct Modifier {
    Stat stat;
    ModifierKind kind;
    double value;          // percent points or flat amount
    std::string source;    // "Ascension 3", "Specialty 2 L5", "Skill: XYZ"
};

class Stats {
public:
    void add(Stat stat, ModifierKind kind, double value, std::string source);

    // Totals
    double total_percent(Stat stat) const; // returns percent points (e.g., 76.0)
    double total_flat(Stat stat) const;

    // Human-friendly combined view:
    // If you pass a Percent stat, you'll usually use total_percent.
    // If you pass a Flat stat, you'll usually use total_flat.
    // But we keep both channels for safety.
    std::vector<Modifier> breakdown(Stat stat, std::optional<ModifierKind> kind = std::nullopt) const;

private:
    std::vector<Modifier> mods_;
};

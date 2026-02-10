// Stats.h
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <iosfwd>

#include "Stat.h"

enum class ModifierKind : int {
    Percent,
    Flat
};

struct Modifier {
    Stat stat;
    ModifierKind kind;
    double value;
    std::string source;
};

class Stats {
public:
    // Store exactly what the DB says. No normalization here.
    void add(Stat stat, ModifierKind kind, double value, std::string source);

    // Raw totals (exact stat only)
    double total_percent(Stat stat) const;
    double total_flat(Stat stat) const;

    // "Effective" totals for GUI/calcs:
    // For base troop stats (Ground/Mounted/Ranged/Siege ATK/DEF/HP), this returns:
    //    base + attacking-only counterpart
    // For all other stats, this equals total_percent().
    double effective_percent(Stat stat) const;

    // Breakdown helpers
    std::vector<Modifier> breakdown(Stat stat, std::optional<ModifierKind> kind = std::nullopt) const;

    // Effective breakdown:
    // For base troop stats, includes BOTH base + attacking-only modifiers (percent kind),
    // while keeping the original stat in each Modifier so the UI can label them.
    std::vector<Modifier> effective_breakdown_percent(Stat stat) const;

    void print_summary(std::ostream& os) const;

private:
    std::vector<Modifier> mods_;
};

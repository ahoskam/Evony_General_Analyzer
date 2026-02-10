// Stats.cpp
#include "Stats.h"
#include <ostream>

static std::optional<Stat> attacking_counterpart_for_base(Stat s)
{
    switch (s) {
        // Ground base -> Ground attacking-only
        case Stat::GroundAttackPct:  return Stat::AttackingGroundAttackPct;
        case Stat::GroundDefensePct: return Stat::AttackingGroundDefensePct;
        case Stat::GroundHPPct:      return Stat::AttackingGroundHPPct;

        // Mounted
        case Stat::MountedAttackPct:  return Stat::AttackingMountedAttackPct;
        case Stat::MountedDefensePct: return Stat::AttackingMountedDefensePct;
        case Stat::MountedHPPct:      return Stat::AttackingMountedHPPct;

        // Ranged
        case Stat::RangedAttackPct:  return Stat::AttackingRangedAttackPct;
        case Stat::RangedDefensePct: return Stat::AttackingRangedDefensePct;
        case Stat::RangedHPPct:      return Stat::AttackingRangedHPPct;

        // Siege
        case Stat::SiegeAttackPct:  return Stat::AttackingSiegeAttackPct;
        case Stat::SiegeDefensePct: return Stat::AttackingSiegeDefensePct;
        case Stat::SiegeHPPct:      return Stat::AttackingSiegeHPPct;

        default:
            return std::nullopt;
    }
}

void Stats::add(Stat stat, ModifierKind kind, double value, std::string source) {
    mods_.push_back(Modifier{stat, kind, value, std::move(source)});
}

double Stats::total_percent(Stat stat) const {
    double sum = 0.0;
    for (const auto& m : mods_) {
        if (m.stat == stat && m.kind == ModifierKind::Percent) sum += m.value;
    }
    return sum;
}

double Stats::total_flat(Stat stat) const {
    double sum = 0.0;
    for (const auto& m : mods_) {
        if (m.stat == stat && m.kind == ModifierKind::Flat) sum += m.value;
    }
    return sum;
}

double Stats::effective_percent(Stat stat) const {
    // Base stat total
    double sum = total_percent(stat);

    // If this stat has an "attacking-only" counterpart, add it
    if (auto atk = attacking_counterpart_for_base(stat)) {
        sum += total_percent(*atk);
    }
    return sum;
}

std::vector<Modifier> Stats::breakdown(Stat stat, std::optional<ModifierKind> kind) const {
    std::vector<Modifier> out;
    out.reserve(8);
    for (const auto& m : mods_) {
        if (m.stat != stat) continue;
        if (kind.has_value() && m.kind != *kind) continue;
        out.push_back(m);
    }
    return out;
}

std::vector<Modifier> Stats::effective_breakdown_percent(Stat stat) const {
    std::vector<Modifier> out;

    // Base percent modifiers
    {
        auto base = breakdown(stat, ModifierKind::Percent);
        out.insert(out.end(), base.begin(), base.end());
    }

    // Attacking-only percent modifiers (if applicable)
    if (auto atk = attacking_counterpart_for_base(stat)) {
        auto extra = breakdown(*atk, ModifierKind::Percent);
        out.insert(out.end(), extra.begin(), extra.end());
    }

    return out;
}

void Stats::print_summary(std::ostream& os) const {
    os << "Ground ATK (effective): " << effective_percent(Stat::GroundAttackPct) << "%\n";
    os << "Ground DEF (effective): " << effective_percent(Stat::GroundDefensePct) << "%\n";
    os << "Ground HP  (effective): " << effective_percent(Stat::GroundHPPct) << "%\n";

    os << "Mounted ATK (effective): " << effective_percent(Stat::MountedAttackPct) << "%\n";
    os << "Mounted DEF (effective): " << effective_percent(Stat::MountedDefensePct) << "%\n";
    os << "Mounted HP  (effective): " << effective_percent(Stat::MountedHPPct) << "%\n";
}

#include "Stats.h"

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

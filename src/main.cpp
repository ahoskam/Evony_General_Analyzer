#include <iostream>
#include <iomanip>

#include "General.h"
#include "Stat.h"
#include "Compute.h"

static void print_stat_percent(const General& g, Stat s) {
    const double total = g.stats().total_percent(s);
    std::cout << "- " << to_string(s) << ": " << total << "%\n";

    auto items = g.stats().breakdown(s, ModifierKind::Percent);
    for (const auto& m : items) {
        std::cout << "    * " << std::setw(6) << m.value << "%  (" << m.source << ")\n";
    }
}

static void print_stat_flat(const General& g, Stat s) {
    const double total = g.stats().total_flat(s);
    std::cout << "- " << to_string(s) << ": +" << total << "\n";

    auto items = g.stats().breakdown(s, ModifierKind::Flat);
    for (const auto& m : items) {
        std::cout << "    * +" << m.value << "  (" << m.source << ")\n";
    }
}

int main() {
     GeneralDefinition lorenzo = load_general_from_file("data/lorenzo.txt");

    GeneralBuild build{};
    build.ascensionLevel = 1;            // your current account
    build.specialtyLevel = {0,0,0,0};    // none for now

    Stats totals = computeStats(lorenzo, build);

    // print a few stats
    std::cout << "Ground Attack total: "
              << totals.total_percent(Stat::GroundAttackPct) << "%\n";
  
}

#pragma once
#include "GeneralDefinition.h"
#include "GeneralBuild.h"
#include "Stats.h"

inline Stats computeStats(
    const GeneralDefinition& def,
    const GeneralBuild& build
) {
    Stats result;

    // Base
    for (const auto& m : def.base)
        result.add(m.stat, m.kind, m.value, m.source);

    // Ascension (cumulative)
    for (const auto& [level, mods] : def.ascension) {
        if (level <= build.ascensionLevel) {
            for (const auto& m : mods)
                result.add(m.stat, m.kind, m.value, m.source);
        }
    }

    // Specialties (cumulative per specialty)
    for (int s = 0; s < 4; ++s) {
        for (int lvl = 0; lvl < build.specialtyLevel[s]; ++lvl) {
            for (const auto& m : def.specialties[s][lvl])
                result.add(m.stat, m.kind, m.value, m.source);
        }
    }

    // Covenants (toggle on/off)
    for (int i = 0; i < 6; ++i) {
        if (build.covenantActive[i] == 0) continue;
        int covNum = i + 1;

        auto it = def.covenants.find(covNum);
        if (it == def.covenants.end()) continue;

        for (const auto& m : it->second)
            result.add(m.stat, m.kind, m.value, m.source);
    }

    return result;
}

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

    // Ascension
    for (const auto& [level, mods] : def.ascension) {
        if (level <= build.ascensionLevel) {
            for (const auto& m : mods)
                result.add(m.stat, m.kind, m.value, m.source);
        }
    }

    // Specialties
    for (int s = 0; s < 4; ++s) {
        for (int lvl = 0; lvl < build.specialtyLevel[s]; ++lvl) {
            for (const auto& m : def.specialties[s][lvl])
                result.add(m.stat, m.kind, m.value, m.source);
        }
    }

    return result;
}

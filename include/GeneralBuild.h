#pragma once
#include <array>
#include "GeneralDefinition.h"

struct GeneralBuild {
    int ascensionLevel = 0;              // 0..5+
    std::array<int, 4> specialtyLevel{}; // each 0..5 (how many levels unlocked)

    // Covenants are usually either active/unlocked or not.
    // 0 = off, 1 = on
    std::array<int, 6> covenantActive{}; // covenants 1..6 -> index 0..5
};

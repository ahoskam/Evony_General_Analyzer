#pragma once
#include <array>
#include "GeneralDefinition.h"

struct GeneralBuild {
    int ascensionLevel = 0;              // 0..5+
    std::array<int, 4> specialtyLevel{}; // each 0..5
};

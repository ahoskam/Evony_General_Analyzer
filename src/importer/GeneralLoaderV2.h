#pragma once
#include <string>
#include <vector>
#include <optional>

struct StatOccurrenceV2 {
    std::string raw_key;
    double value = 0.0;

    std::string context_type; // BaseSkill / Ascension / Specialty / Covenant
    std::string context_name;
    std::optional<int> level;
    bool is_total = false;

    std::string file_path;
    int line_number = 0;
    std::string raw_line;
};

struct GeneralMetaV2 {
    std::string name;
    std::string role;
    bool in_tavern = false;
    std::string base_skill_name;

    int leadership = 0;     double leadership_green = 0.0;
    int attack = 0;         double attack_green = 0.0;
    int defense = 0;        double defense_green = 0.0;
    int politics = 0;       double politics_green = 0.0;

    // NEW
    std::string source_text_verbatim;

    // NEW (default false)
    bool double_checked_in_game = false;
};

struct LoadedGeneralV2 {
    GeneralMetaV2 meta;
    std::vector<StatOccurrenceV2> occurrences;
    std::vector<std::string> errors;
};

LoadedGeneralV2 load_general_v2_from_file(const std::string& path);

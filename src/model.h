#pragma once
#include <string>
#include <vector>
#include <optional>

struct GeneralRow {
  int id = 0;
  std::string name;
  std::string role;
  int double_checked_in_game = 0;
};

struct GeneralMeta {
  int id = 0;
  std::string name;
  std::string role;
  int in_tavern = 0;
  std::string base_skill_name;

  int leadership = 0; double leadership_green = 0;
  int attack = 0;     double attack_green = 0;
  int defense = 0;    double defense_green = 0;
  int politics = 0;   double politics_green = 0;

  int role_confirmed = 1;
  std::string source_text_verbatim;
  int double_checked_in_game = 0;
};

struct Occurrence {
  int id = 0;
  int general_id = 0;
  int stat_key_id = 0;
  std::string stat_key; // joined stat_keys.key
  double value = 0;

  std::string context_type;
  std::string context_name;
  std::optional<int> level;
  int is_total = 0;

  std::string file_path;
  int line_number = 0;
  std::string raw_line;

  // v2 columns
  std::string origin;                 // imported/generated/manual
  std::optional<int> generated_from_total_id;
  int edited_by_user = 0;
};

struct StatKey {
  int id = 0;
  std::string name; // stat_keys.key
};

struct PendingExample {
  int pending_id = 0;
  std::string raw_key;
  double value = 0;

  std::string context_type;
  std::string context_name;

  // Optional if your pending table stores it (it does in importer insert)
  std::optional<int> level;

  std::string file_path;
  int line_number = 0;
  std::string raw_line;
};

struct LoadAllResult {
  GeneralMeta meta;
  std::vector<Occurrence> occ;
  std::vector<PendingExample> pending;
};

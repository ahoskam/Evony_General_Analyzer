#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

struct GeneralRow {
  int id = 0;
  std::string name;
  std::string role;
  int double_checked_in_game = 0;
  int status_flags = 0;
  int covenant_max = 0; // 0 if none
};

enum GeneralStatusFlags : int {
  GS_MISSING_SOURCE_TEXT   = 1 << 0,
  GS_MISSING_BASE_SKILL    = 1 << 1,
  GS_MISSING_ASCENSIONS    = 1 << 2,
  GS_MISSING_SPECIALTIES   = 1 << 3,
  GS_MISSING_COVENANT_6    = 1 << 4,
};


struct GeneralMeta {
  int id = 0;
  std::string name;
  std::string role;
  std::string country = "Unknown";
  int has_covenant = 0;
  std::string covenant_member_1;
  std::string covenant_member_2;
  std::string covenant_member_3;
  int in_tavern = 0;
  std::string base_skill_name;

  int leadership = 0; double leadership_green = 0;
  int attack = 0;     double attack_green = 0;
  int defense = 0;    double defense_green = 0;
  int politics = 0;   double politics_green = 0;

  int role_confirmed = 1;
  std::string source_text_verbatim;
  int double_checked_in_game = 0;

  // Optional portrait/media stored directly in DB.
  std::vector<std::uint8_t> image_blob;
  std::string image_mime;
  std::string image_filename;
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
  std::string origin;                 // imported/generated/gui
  std::optional<int> generated_from_total_id;
  int edited_by_user = 0;
  int stat_checked_in_game = 0;
};

struct StatKey {
  int id = 0;
  std::string name; // stat_keys.key
  int is_active = 1; // 1=canonical/active, 0=legacy/inactive
};

struct PendingExample {
  int id = 0;
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

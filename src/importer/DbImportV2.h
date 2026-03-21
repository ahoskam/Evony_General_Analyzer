#pragma once
#include <optional>
#include <sqlite3.h>
#include <string>

class DbImportV2 {
public:
  struct PendingInfo {
    int pending_id = 0;
    bool was_new = false;
  };

  DbImportV2() = default;
  ~DbImportV2();

  DbImportV2(const DbImportV2 &) = delete;
  DbImportV2 &operator=(const DbImportV2 &) = delete;

  bool open(const std::string &path);
  void close();
  sqlite3 *raw() const { return db_; }

  bool begin();
  bool commit();
  bool rollback();

  // NOTE: importer rules say importer must not set double_checked_in_game=1
  // from file input; only GUI can. This parameter is accepted but should not
  // force lock = true.
  bool upsert_general(const std::string &name, const std::string &role,
                      const std::string &country,
                      bool has_covenant,
                      const std::string &covenant_member_1,
                      const std::string &covenant_member_2,
                      const std::string &covenant_member_3,
                      bool role_confirmed, bool in_tavern,
                      const std::string &base_skill_name, int leadership,
                      double leadership_green, int attack, double attack_green,
                      int defense, double defense_green, int politics,
                      double politics_green,
                      const std::string &source_text_verbatim,
                      bool double_checked_in_game, int &out_general_id);

  std::optional<int> resolve_stat_key_id(const std::string &raw_key);

  bool ensure_pending_key(const std::string &raw_key,
                          const std::string &first_seen_file,
                          int first_seen_line, PendingInfo &out);

  bool add_pending_example(int pending_id, const std::string &general_name,
                           const std::string &context_type,
                           const std::string &context_name,
                           const std::optional<int> &level, double value,
                           const std::string &file_path, int line_number,
                           const std::string &raw_line);

  bool insert_stat_occurrence(int general_id, int stat_key_id, double value,
                              const std::string &context_type,
                              const std::string &context_name,
                              const std::optional<int> &level, bool is_total,
                              const std::string &file_path, int line_number,
                              const std::string &raw_line);

  bool delete_occurrences_for_general(int general_id);
  bool delete_occurrences_for_general_file(int general_id,
                                           const std::string &file_path);

private:
  sqlite3 *db_ = nullptr;

  bool exec_sql(const char *sql);

  bool get_general_lock_status(const std::string &name, bool &found,
                               int &general_id, bool &locked);
};

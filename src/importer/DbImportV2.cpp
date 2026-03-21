#include "DbImportV2.h"
#include "../db_maintenance.h"
#include "../role_utils.h"
#include <iostream>
#include <string>
#include <cctype>

static std::string normalize_general_country(const std::string& country) {
    std::string c = country;
    for (auto& ch : c) ch = (char)std::tolower((unsigned char)ch);
    if (c == "europe") return "Europe";
    if (c == "america") return "America";
    if (c == "japan") return "Japan";
    if (c == "korea") return "Korea";
    if (c == "china") return "China";
    if (c == "russia") return "Russia";
    if (c == "arabia") return "Arabia";
    if (c == "other") return "Other";
    return "Unknown";
}

DbImportV2::~DbImportV2() { close(); }

bool DbImportV2::exec_sql(const char* sql)
{
    if (!db_) return false;

    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "SQLite error: " << (err ? err : "(unknown)") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool DbImportV2::open(const std::string& path)
{
    close();

    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "sqlite3_open failed: " << sqlite3_errmsg(db_) << "\n";
        close();
        return false;
    }

    exec_sql("PRAGMA foreign_keys = ON;");
    exec_sql("PRAGMA journal_mode = WAL;");

    std::string err;
    if (!db_apply_generals_migrations(db_, &err)) {
        std::cerr << "DB migrations failed: " << err << "\n";
        close();
        return false;
    }

    return true;
}

void DbImportV2::close()
{
    if (db_) sqlite3_close(db_);
    db_ = nullptr;
}

bool DbImportV2::begin()    { return exec_sql("BEGIN;"); }
bool DbImportV2::commit()   { return exec_sql("COMMIT;"); }
bool DbImportV2::rollback() { return exec_sql("ROLLBACK;"); }

// -----------------------------------------------------------------------------
// General locking / policy enforcement
// -----------------------------------------------------------------------------

// Returns:
//  - found=false if general doesn't exist yet
//  - if found=true, provides id and locked (double_checked_in_game==1)
bool DbImportV2::get_general_lock_status(
    const std::string& name,
    bool& found,
    int& general_id,
    bool& locked)
{
    found = false;
    general_id = 0;
    locked = false;

    if (!db_) return false;

    const char* sql = R"SQL(
        SELECT id, double_checked_in_game
        FROM generals
        WHERE name=?1;
    )SQL";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare get_general_lock_status failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        found = true;
        general_id = sqlite3_column_int(stmt, 0);
        locked = (sqlite3_column_int(stmt, 1) == 1);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool DbImportV2::upsert_general(
    const std::string& name,
    const std::string& role,
    const std::string& country,
    bool has_covenant,
    const std::string& covenant_member_1,
    const std::string& covenant_member_2,
    const std::string& covenant_member_3,
    bool role_confirmed,
    bool in_tavern,
    const std::string& base_skill_name,
    int leadership, double leadership_green,
    int attack, double attack_green,
    int defense, double defense_green,
    int politics, double politics_green,
    const std::string& source_text_verbatim,
    bool /*double_checked_in_game_from_file_ignored*/,
    int& out_general_id)
{
    if (!db_) return false;
    const std::string normalized_role = normalize_general_role(role);
    const std::string normalized_country = normalize_general_country(country);

    // Enforce project rule:
    // - If general is locked (double_checked_in_game==1), importer MUST skip all updates.
    // - If general is unlocked, importer MUST set double_checked_in_game=0 unconditionally.
    bool found = false;
    bool locked = false;
    int existing_id = 0;

    if (!get_general_lock_status(name, found, existing_id, locked)) return false;

    if (found && locked) {
        out_general_id = existing_id;
        return true; // Skip updates.
    }

    // IMPORTANT:
    // We never set double_checked_in_game=1 from file. Always 0 on insert/update by importer.
    const int importer_sets_double_checked = 0;

    const char* sql = R"SQL(
        INSERT INTO generals(
            name, role, country, has_covenant, covenant_member_1, covenant_member_2, covenant_member_3,
            role_confirmed, in_tavern, base_skill_name,
            leadership, leadership_green,
            attack, attack_green,
            defense, defense_green,
            politics, politics_green,
            source_text_verbatim,
            double_checked_in_game
        ) VALUES (
            ?1, ?2, ?3, ?4, ?5, ?6, ?7,
            ?8, ?9, ?10,
            ?11, ?12,
            ?13, ?14,
            ?15, ?16,
            ?17, ?18,
            ?19,
            ?20
        )
        ON CONFLICT(name) DO UPDATE SET
            role=excluded.role,
            country=excluded.country,
            has_covenant=excluded.has_covenant,
            covenant_member_1=excluded.covenant_member_1,
            covenant_member_2=excluded.covenant_member_2,
            covenant_member_3=excluded.covenant_member_3,
            role_confirmed=excluded.role_confirmed,
            in_tavern=excluded.in_tavern,
            base_skill_name=excluded.base_skill_name,
            leadership=excluded.leadership,
            leadership_green=excluded.leadership_green,
            attack=excluded.attack,
            attack_green=excluded.attack_green,
            defense=excluded.defense,
            defense_green=excluded.defense_green,
            politics=excluded.politics,
            politics_green=excluded.politics_green,
            source_text_verbatim=excluded.source_text_verbatim,
            double_checked_in_game=excluded.double_checked_in_game;
    )SQL";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare upsert_general failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, normalized_role.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, normalized_country.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 4, has_covenant ? 1 : 0);
    sqlite3_bind_text(stmt, 5, covenant_member_1.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, covenant_member_2.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, covenant_member_3.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 8, role_confirmed ? 1 : 0);
    sqlite3_bind_int (stmt, 9, in_tavern ? 1 : 0);
    sqlite3_bind_text(stmt, 10, base_skill_name.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_int   (stmt, 11, leadership);
    sqlite3_bind_double(stmt, 12, leadership_green);

    sqlite3_bind_int   (stmt, 13, attack);
    sqlite3_bind_double(stmt, 14, attack_green);

    sqlite3_bind_int   (stmt, 15, defense);
    sqlite3_bind_double(stmt, 16, defense_green);

    sqlite3_bind_int   (stmt, 17, politics);
    sqlite3_bind_double(stmt, 18, politics_green);

    sqlite3_bind_text(stmt, 19, source_text_verbatim.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 20, importer_sets_double_checked);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) std::cerr << "upsert_general failed: " << sqlite3_errmsg(db_) << "\n";
    sqlite3_finalize(stmt);
    if (!ok) return false;

    // Fetch id
    const char* sel = "SELECT id FROM generals WHERE name=?1;";
    if (sqlite3_prepare_v2(db_, sel, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare select id failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        out_general_id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return true;
    }

    std::cerr << "select general id failed: " << sqlite3_errmsg(db_) << "\n";
    sqlite3_finalize(stmt);
    return false;
}

std::optional<int> DbImportV2::resolve_stat_key_id(const std::string& raw_key)
{
    if (!db_) return std::nullopt;

    // 1) alias match
    {
        const char* sql =
            "SELECT a.stat_key_id "
            "FROM stat_key_aliases a "
            "JOIN stat_keys k ON k.id = a.stat_key_id "
            "WHERE a.alias_key=?1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "prepare failed: " << sqlite3_errmsg(db_) << "\n";
            return std::nullopt;
        }
        sqlite3_bind_text(stmt, 1, raw_key.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            return id;
        }
        sqlite3_finalize(stmt);
    }

    // 2) exact canonical key match
    {
        const char* sql = "SELECT id FROM stat_keys WHERE key=?1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "prepare failed: " << sqlite3_errmsg(db_) << "\n";
            return std::nullopt;
        }
        sqlite3_bind_text(stmt, 1, raw_key.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            return id;
        }
        sqlite3_finalize(stmt);
    }

    return std::nullopt;
}

bool DbImportV2::ensure_pending_key(
    const std::string& raw_key,
    const std::string& first_seen_file,
    int first_seen_line,
    PendingInfo& out)
{
    if (!db_) return false;

    const char* ins = R"SQL(
        INSERT INTO pending_stat_keys(raw_key, status, first_seen_file, first_seen_line)
        VALUES(?1, 'pending', ?2, ?3);
    )SQL";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, ins, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, raw_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, first_seen_file.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, first_seen_line);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        out.pending_id = (int)sqlite3_last_insert_rowid(db_);
        out.was_new = true;
        return true;
    }

    // If insert failed (likely UNIQUE), fetch existing id.
    const char* sel = "SELECT id FROM pending_stat_keys WHERE raw_key=?1;";
    if (sqlite3_prepare_v2(db_, sel, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, raw_key.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        out.pending_id = sqlite3_column_int(stmt, 0);
        out.was_new = false;
        sqlite3_finalize(stmt);
        return true;
    }

    std::cerr << "ensure_pending_key failed: " << sqlite3_errmsg(db_) << "\n";
    sqlite3_finalize(stmt);
    return false;
}

bool DbImportV2::add_pending_example(
    int pending_id,
    const std::string& general_name,
    const std::string& context_type,
    const std::string& context_name,
    const std::optional<int>& level,
    double value,
    const std::string& file_path,
    int line_number,
    const std::string& raw_line)
{
    if (!db_) return false;

    const char* sql = R"SQL(
        INSERT OR IGNORE INTO pending_stat_key_examples(
            pending_id, general_name, context_type, context_name, level, value,
            file_path, line_number, raw_line
        ) VALUES (
            ?1, ?2, ?3, ?4, ?5, ?6,
            ?7, ?8, ?9
        );
    )SQL";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }

    sqlite3_bind_int (stmt, 1, pending_id);
    sqlite3_bind_text(stmt, 2, general_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, context_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, context_name.c_str(), -1, SQLITE_TRANSIENT);

    if (level.has_value()) sqlite3_bind_int(stmt, 5, *level);
    else sqlite3_bind_null(stmt, 5);

    sqlite3_bind_double(stmt, 6, value);
    sqlite3_bind_text  (stmt, 7, file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int   (stmt, 8, line_number);
    sqlite3_bind_text  (stmt, 9, raw_line.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    const bool ok = (rc == SQLITE_DONE);

    if (!ok) {
        std::cerr << "add_pending_example failed: " << sqlite3_errmsg(db_) << "\n";
    }

    sqlite3_finalize(stmt);
    return ok;
}

// -----------------------------------------------------------------------------
// IMPORTANT FIX: delete by general_id (NOT by file_path)
// This prevents duplicates when a general is imported from data/import and
// later from data/imported (or any other path).
// -----------------------------------------------------------------------------
bool DbImportV2::delete_occurrences_for_general(int general_id)
{
    if (!db_) return false;

    const char* sql = "DELETE FROM stat_occurrences WHERE general_id=?1;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare delete_occurrences_for_general failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }

    sqlite3_bind_int(stmt, 1, general_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) std::cerr << "delete_occurrences_for_general failed: " << sqlite3_errmsg(db_) << "\n";
    sqlite3_finalize(stmt);
    return ok;
}

// Keep the old helper if you still want it, but do NOT use it for idempotent imports.
bool DbImportV2::delete_occurrences_for_general_file(int general_id, const std::string& file_path)
{
    if (!db_) return false;

    const char* sql = "DELETE FROM stat_occurrences WHERE general_id=?1 AND file_path=?2;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }

    sqlite3_bind_int(stmt, 1, general_id);
    sqlite3_bind_text(stmt, 2, file_path.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) std::cerr << "delete_occurrences_for_general_file failed: " << sqlite3_errmsg(db_) << "\n";
    sqlite3_finalize(stmt);
    return ok;
}

bool DbImportV2::insert_stat_occurrence(
    int general_id,
    int stat_key_id,
    double value,
    const std::string& context_type,
    const std::string& context_name,
    const std::optional<int>& level,
    bool is_total,
    const std::string& file_path,
    int line_number,
    const std::string& raw_line)
{
    if (!db_) return false;

    const char* sql = R"SQL(
        INSERT INTO stat_occurrences(
            general_id, stat_key_id, value,
            context_type, context_name, level, is_total,
            file_path, line_number, raw_line
        ) VALUES (
            ?1, ?2, ?3,
            ?4, ?5, ?6, ?7,
            ?8, ?9, ?10
        );
    )SQL";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }

    sqlite3_bind_int(stmt, 1, general_id);
    sqlite3_bind_int(stmt, 2, stat_key_id);
    sqlite3_bind_double(stmt, 3, value);

    sqlite3_bind_text(stmt, 4, context_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, context_name.c_str(), -1, SQLITE_TRANSIENT);

    if (level.has_value()) sqlite3_bind_int(stmt, 6, *level);
    else sqlite3_bind_null(stmt, 6);

    sqlite3_bind_int(stmt, 7, is_total ? 1 : 0);

    sqlite3_bind_text(stmt, 8, file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 9, line_number);
    sqlite3_bind_text(stmt, 10, raw_line.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) {
        std::cerr << "insert_stat_occurrence failed: " << sqlite3_errmsg(db_)
                  << " | general_id=" << general_id
                  << " | stat_key_id=" << stat_key_id
                  << " | file=" << file_path << ":" << line_number
                  << " | context=" << context_type << "/" << context_name
                  << "\n";
    }
    sqlite3_finalize(stmt);
    return ok;
}

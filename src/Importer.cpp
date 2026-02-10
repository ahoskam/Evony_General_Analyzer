#include "Importer.h"
#include "GeneralLoader.h"
#include "Db.h"
#include "StatParse.h"

#include <sqlite3.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static bool exec_sql(sqlite3* db, const char* sql)
{
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "SQLite error: " << (err ? err : "(unknown)") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

static int get_general_id(sqlite3* db, const std::string& name)
{
    const char* q = "SELECT id FROM generals WHERE name=?1;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, q, -1, &st, nullptr) != SQLITE_OK) return -1;

    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    int id = -1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        id = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    return id;
}

static int ensure_general(sqlite3* db, const std::string& name, const std::string& role)
{
    int id = get_general_id(db, name);
    if (id != -1) return id;

    const char* ins = "INSERT INTO generals(name, role) VALUES (?1, ?2);";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, ins, -1, &st, nullptr) != SQLITE_OK) return -1;

    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, role.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(st) != SQLITE_DONE) {
        std::cerr << "Insert general failed: " << sqlite3_errmsg(db) << "\n";
        sqlite3_finalize(st);
        return -1;
    }
    sqlite3_finalize(st);

    return get_general_id(db, name);
}

static int get_stat_key_id(sqlite3* db, const std::string& key)
{
    const char* q = "SELECT id FROM stat_keys WHERE key=?1;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, q, -1, &st, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    int id = -1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        id = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    return id;
}

static bool insert_modifier_base(sqlite3* db, int general_id, int stat_key_id, double value, const std::string& raw)
{
    const char* ins = R"SQL(
      INSERT INTO modifiers(general_id, stat_key_id, source_type, value, raw_text)
      VALUES (?1, ?2, 'base', ?3, ?4);
    )SQL";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, ins, -1, &st, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(st, 1, general_id);
    sqlite3_bind_int(st, 2, stat_key_id);
    sqlite3_bind_double(st, 3, value);
    sqlite3_bind_text(st, 4, raw.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(st) == SQLITE_DONE);
    if (!ok) std::cerr << "Insert modifier failed: " << sqlite3_errmsg(db) << "\n";
    sqlite3_finalize(st);
    return ok;
}

static bool insert_modifier_asc(sqlite3* db, int general_id, int stat_key_id, int asc_level, double value, const std::string& raw)
{
    const char* ins = R"SQL(
      INSERT INTO modifiers(general_id, stat_key_id, source_type, ascension_level, value, raw_text)
      VALUES (?1, ?2, 'ascension', ?3, ?4, ?5);
    )SQL";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, ins, -1, &st, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(st, 1, general_id);
    sqlite3_bind_int(st, 2, stat_key_id);
    sqlite3_bind_int(st, 3, asc_level);
    sqlite3_bind_double(st, 4, value);
    sqlite3_bind_text(st, 5, raw.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(st) == SQLITE_DONE);
    if (!ok) std::cerr << "Insert modifier failed: " << sqlite3_errmsg(db) << "\n";
    sqlite3_finalize(st);
    return ok;
}

static bool insert_modifier_spec(sqlite3* db, int general_id, int stat_key_id, int spec_num, int spec_level, double value, const std::string& raw)
{
    const char* ins = R"SQL(
      INSERT INTO modifiers(general_id, stat_key_id, source_type, specialty_number, specialty_level, value, raw_text)
      VALUES (?1, ?2, 'specialty', ?3, ?4, ?5, ?6);
    )SQL";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, ins, -1, &st, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(st, 1, general_id);
    sqlite3_bind_int(st, 2, stat_key_id);
    sqlite3_bind_int(st, 3, spec_num);
    sqlite3_bind_int(st, 4, spec_level);
    sqlite3_bind_double(st, 5, value);
    sqlite3_bind_text(st, 6, raw.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(st) == SQLITE_DONE);
    if (!ok) std::cerr << "Insert modifier failed: " << sqlite3_errmsg(db) << "\n";
    sqlite3_finalize(st);
    return ok;
}

static bool insert_modifier_cov(sqlite3* db, int general_id, int stat_key_id, int cov_num, const std::string& cov_name, double value, const std::string& raw)
{
    const char* ins = R"SQL(
      INSERT INTO modifiers(general_id, stat_key_id, source_type, covenant_number, covenant_name, value, raw_text)
      VALUES (?1, ?2, 'covenant', ?3, ?4, ?5, ?6);
    )SQL";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, ins, -1, &st, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(st, 1, general_id);
    sqlite3_bind_int(st, 2, stat_key_id);
    sqlite3_bind_int(st, 3, cov_num);
    sqlite3_bind_text(st, 4, cov_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 5, value);
    sqlite3_bind_text(st, 6, raw.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(st) == SQLITE_DONE);
    if (!ok) std::cerr << "Insert modifier failed: " << sqlite3_errmsg(db) << "\n";
    sqlite3_finalize(st);
    return ok;
}

bool import_generals_from_folder(sqlite3* db, const std::string& import_dir)
{
    if (!db) return false;

    if (!fs::exists(import_dir)) {
        std::cout << "Import folder not found: " << import_dir << "\n";
        return true; // not an error
    }

    int imported = 0;
    int skipped = 0;

    for (const auto& entry : fs::directory_iterator(import_dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".txt") continue;

        const std::string path = entry.path().string();

        GeneralDefinition def;
        try {
            def = load_general_from_file(path);
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse " << path << ": " << e.what() << "\n";
            continue;
        }

        // If already in DB, skip (two-pronged behavior)
        if (get_general_id(db, def.name) != -1) {
            ++skipped;
            continue;
        }

        // Import in one transaction
        if (!exec_sql(db, "BEGIN;")) return false;

        int general_id = ensure_general(db, def.name, def.role);
        if (general_id == -1) {
            exec_sql(db, "ROLLBACK;");
            continue;
        }

        auto insert_mod = [&](const Modifier& m, const char* raw,
                              const char* source_type,
                              int asc, int spec_num, int spec_lvl, int cov_num, const std::string& cov_name) -> bool {
            (void)source_type;

            std::string_view keysv = key_from_stat(m.stat);
            if (keysv.empty()) return true; // stat not mapped to key (shouldn't happen for supported stats)

            std::string key(keysv);
            int stat_key_id = get_stat_key_id(db, key);
            if (stat_key_id == -1) {
                // If key isn't in DB yet, you either forgot sync or it's truly new.
                // Sync should prevent this, but we fail loudly.
                std::cerr << "Missing stat_keys row for key: " << key << "\n";
                return false;
            }

            std::string rawLine = raw ? raw : (key + " " + std::to_string(m.value));

            if (asc > 0) return insert_modifier_asc(db, general_id, stat_key_id, asc, m.value, rawLine);
            if (spec_num > 0) return insert_modifier_spec(db, general_id, stat_key_id, spec_num, spec_lvl, m.value, rawLine);
            if (cov_num > 0) return insert_modifier_cov(db, general_id, stat_key_id, cov_num, cov_name, m.value, rawLine);
            return insert_modifier_base(db, general_id, stat_key_id, m.value, rawLine);
        };

        bool ok = true;

        // Base
        for (const auto& m : def.base) {
            if (!insert_mod(m, m.source.c_str(), "base", 0, 0, 0, 0, "")) { ok = false; break; }
        }

        // Ascensions
        if (ok) {
            for (const auto& [lvl, mods] : def.ascension) {
                for (const auto& m : mods) {
                    if (!insert_mod(m, m.source.c_str(), "ascension", lvl, 0, 0, 0, "")) { ok = false; break; }
                }
                if (!ok) break;
            }
        }

        // Specialties
        if (ok) {
            for (int s = 0; s < 4; ++s) {
                for (int l = 0; l < 5; ++l) {
                    for (const auto& m : def.specialties[s][l]) {
                        if (!insert_mod(m, m.source.c_str(), "specialty", 0, s + 1, l + 1, 0, "")) { ok = false; break; }
                    }
                    if (!ok) break;
                }
                if (!ok) break;
            }
        }

        // Covenants
        if (ok) {
            for (const auto& [covNum, mods] : def.covenants) {
                std::string covName = "Covenant " + std::to_string(covNum);
                auto itn = def.covenantNames.find(covNum);
                if (itn != def.covenantNames.end() && !itn->second.empty()) covName = itn->second;

                for (const auto& m : mods) {
                    if (!insert_mod(m, m.source.c_str(), "covenant", 0, 0, 0, covNum, covName)) { ok = false; break; }
                }
                if (!ok) break;
            }
        }

        if (!ok) {
            exec_sql(db, "ROLLBACK;");
            std::cerr << "Import failed for general: " << def.name << "\n";
            continue;
        }

        if (!exec_sql(db, "COMMIT;")) {
            exec_sql(db, "ROLLBACK;");
            continue;
        }

        ++imported;
        std::cout << "Imported general: " << def.name << " (" << def.role << ")\n";
    }

    std::cout << "Import scan complete. Imported=" << imported << " Skipped(existing)=" << skipped << "\n";
    return true;
}

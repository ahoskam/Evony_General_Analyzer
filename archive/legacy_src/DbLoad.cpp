#include "DbLoad.h"
#include "StatParse.h"

#include <sqlite3.h>
#include <stdexcept>
#include <iostream>

static void throw_sql(sqlite3* db, const std::string& msg) {
    throw std::runtime_error(msg + " | sqlite: " + sqlite3_errmsg(db));
}

std::vector<std::string> db_list_generals(sqlite3* db)
{
    std::vector<std::string> out;

    const char* sql = R"SQL(
        SELECT name
        FROM generals
        ORDER BY name;
    )SQL";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        throw_sql(db, "prepare failed in db_list_generals()");
    }

    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* txt = sqlite3_column_text(st, 0);
        out.emplace_back(txt ? (const char*)txt : "");
    }

    sqlite3_finalize(st);
    return out;
}

GeneralDefinition db_load_general(sqlite3* db, const std::string& general_name)
{
    GeneralDefinition def;

    // 1) Get general id + role
    {
        const char* sql = R"SQL(
            SELECT id, name, role
            FROM generals
            WHERE name = ?1;
        )SQL";

        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
            throw_sql(db, "prepare failed fetching general row");
        }

        sqlite3_bind_text(st, 1, general_name.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(st);
        if (rc != SQLITE_ROW) {
            sqlite3_finalize(st);
            throw std::runtime_error("General not found in DB: " + general_name);
        }

        // int id = sqlite3_column_int(st, 0);
        const unsigned char* nameTxt = sqlite3_column_text(st, 1);
        const unsigned char* roleTxt = sqlite3_column_text(st, 2);

        def.name = nameTxt ? (const char*)nameTxt : general_name;
        def.role = roleTxt ? (const char*)roleTxt : "Unknown";

        sqlite3_finalize(st);
    }

    // 2) Load modifiers
    //
    // We join modifiers -> stat_keys to get the string key, then map to enum Stat
    // using stat_from_key(). Unknown keys are skipped (so the app won't crash if
    // the DB contains keys you haven't implemented yet).
    {
        const char* sql = R"SQL(
            SELECT
              sk.key,
              m.value,
              m.source_type,
              m.ascension_level,
              m.specialty_number,
              m.specialty_level,
              m.covenant_number,
              m.covenant_name
            FROM modifiers m
            JOIN generals g   ON g.id = m.general_id
            JOIN stat_keys sk ON sk.id = m.stat_key_id
            WHERE g.name = ?1
            ORDER BY m.id ASC;
        )SQL";

        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
            throw_sql(db, "prepare failed fetching modifiers");
        }

        sqlite3_bind_text(st, 1, general_name.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char* keyTxt = sqlite3_column_text(st, 0);
            double value = sqlite3_column_double(st, 1);

            const unsigned char* srcTypeTxt = sqlite3_column_text(st, 2);
            int asc = sqlite3_column_type(st, 3) == SQLITE_NULL ? 0 : sqlite3_column_int(st, 3);
            int specNum = sqlite3_column_type(st, 4) == SQLITE_NULL ? 0 : sqlite3_column_int(st, 4);
            int specLvl = sqlite3_column_type(st, 5) == SQLITE_NULL ? 0 : sqlite3_column_int(st, 5);
            int covNum = sqlite3_column_type(st, 6) == SQLITE_NULL ? 0 : sqlite3_column_int(st, 6);

            const unsigned char* covNameTxt = sqlite3_column_text(st, 7);

            std::string key = keyTxt ? (const char*)keyTxt : "";
            std::string srcType = srcTypeTxt ? (const char*)srcTypeTxt : "";

            auto statOpt = stat_from_key(key);
            if (!statOpt.has_value()) {
                // Not implemented in your parser yet; skip instead of crashing.
                continue;
            }

            Stat stat = *statOpt;

            // Build a nice source string for debugging/UI
            std::string source;
            if (srcType == "base") source = "Base";
            else if (srcType == "ascension") source = "Ascension " + std::to_string(asc);
            else if (srcType == "specialty") source = "Specialty " + std::to_string(specNum) + " L" + std::to_string(specLvl);
            else if (srcType == "covenant") {
                std::string cn = covNameTxt ? (const char*)covNameTxt : ("Covenant " + std::to_string(covNum));
                source = "Covenant " + std::to_string(covNum) + ": " + cn;
                def.covenantNames[covNum] = cn;
            } else source = "Unknown";

            Modifier m{stat, kind_for_stat(stat), value, source};

            // Place modifier into the right bucket
            if (srcType == "base") {
                def.base.push_back(std::move(m));
            } else if (srcType == "ascension") {
                def.ascension[asc].push_back(std::move(m));
            } else if (srcType == "specialty") {
                // DB stores 1..4 and 1..5; convert to 0-based
                if (specNum >= 1 && specNum <= 4 && specLvl >= 1 && specLvl <= 5) {
                    def.specialties[specNum - 1][specLvl - 1].push_back(std::move(m));
                }
            } else if (srcType == "covenant") {
                if (covNum >= 1 && covNum <= 6) {
                    def.covenants[covNum].push_back(std::move(m));
                }
            }
        }

        sqlite3_finalize(st);
    }

    return def;
}

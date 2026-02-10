#include "Db.h"
#include "StatParse.h"

#include <sqlite3.h>
#include <iostream>

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

sqlite3* db_open(const std::string& path)
{
    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "sqlite3_open failed: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return nullptr;
    }
    exec_sql(db, "PRAGMA foreign_keys = ON;");
    exec_sql(db, "PRAGMA journal_mode = WAL;");
    return db;
}

void db_close(sqlite3* db)
{
    if (db) sqlite3_close(db);
}

bool db_ensure_schema(sqlite3* db)
{
    if (!db) return false;

    const char* schema = R"SQL(
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS generals (
          id    INTEGER PRIMARY KEY AUTOINCREMENT,
          name  TEXT NOT NULL UNIQUE,
          role  TEXT NOT NULL,
          notes TEXT
        );

        CREATE TABLE IF NOT EXISTS stat_keys (
          id    INTEGER PRIMARY KEY AUTOINCREMENT,
          key   TEXT NOT NULL UNIQUE,
          kind  TEXT NOT NULL CHECK (kind IN ('percent','flat')),
          label TEXT
        );

        CREATE TABLE IF NOT EXISTS modifiers (
          id              INTEGER PRIMARY KEY AUTOINCREMENT,
          general_id       INTEGER NOT NULL,
          stat_key_id      INTEGER NOT NULL,

          source_type      TEXT NOT NULL CHECK (source_type IN ('base','ascension','specialty','covenant')),

          ascension_level  INTEGER CHECK (ascension_level BETWEEN 1 AND 5),

          specialty_number INTEGER CHECK (specialty_number BETWEEN 1 AND 4),
          specialty_level  INTEGER CHECK (specialty_level BETWEEN 1 AND 5),

          covenant_number  INTEGER CHECK (covenant_number BETWEEN 1 AND 6),
          covenant_name    TEXT,

          value            REAL NOT NULL,
          raw_text         TEXT,

          FOREIGN KEY (general_id)  REFERENCES generals(id)   ON DELETE CASCADE,
          FOREIGN KEY (stat_key_id) REFERENCES stat_keys(id)  ON DELETE RESTRICT
        );

        CREATE INDEX IF NOT EXISTS idx_modifiers_general
          ON modifiers(general_id);

        CREATE INDEX IF NOT EXISTS idx_modifiers_lookup
          ON modifiers(general_id, source_type, ascension_level, specialty_number, specialty_level, covenant_number);

        CREATE INDEX IF NOT EXISTS idx_stat_keys_key
          ON stat_keys(key);
    )SQL";

    return exec_sql(db, schema);
}

bool db_sync_stat_keys(sqlite3* db)
{
    if (!db) return false;

    // Upsert: key/kind/label
    const char* upsert_sql = R"SQL(
        INSERT INTO stat_keys(key, kind, label)
        VALUES (?1, ?2, ?3)
        ON CONFLICT(key) DO UPDATE SET
          kind  = excluded.kind,
          label = excluded.label;
    )SQL";

    if (!exec_sql(db, "BEGIN;")) return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, upsert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare failed: " << sqlite3_errmsg(db) << "\n";
        exec_sql(db, "ROLLBACK;");
        return false;
    }

    int count = 0;
    for (auto key : supported_stat_keys()) {
        auto statOpt = stat_from_key(std::string(key));
        if (!statOpt.has_value()) continue;

        const char* kind = (kind_for_stat(*statOpt) == ModifierKind::Flat) ? "flat" : "percent";

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);

        sqlite3_bind_text(stmt, 1, key.data(), (int)key.size(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, kind, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, key.data(), (int)key.size(), SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "upsert failed for " << std::string(key) << ": " << sqlite3_errmsg(db) << "\n";
            sqlite3_finalize(stmt);
            exec_sql(db, "ROLLBACK;");
            return false;
        }
        ++count;
    }

    sqlite3_finalize(stmt);

    if (!exec_sql(db, "COMMIT;")) return false;

    std::cout << "Synced " << count << " stat_keys into the database.\n";
    return true;
}

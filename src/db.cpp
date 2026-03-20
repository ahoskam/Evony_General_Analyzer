#include "db.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

bool column_exists(sqlite3* db, const char* table, const char* column) {
  std::string sql = "PRAGMA table_info(" + std::string(table) + ");";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }

  auto lower = [](std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
  };

  const std::string want = lower(column);
  bool found = false;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char* txt = sqlite3_column_text(stmt, 1);
    std::string name = txt ? (const char*)txt : "";
    if (lower(name) == want) { found = true; break; }
  }
  sqlite3_finalize(stmt);
  return found;
}

void exec_migration_sql(sqlite3* db, const char* sql) {
  char* err = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
    const std::string msg = err ? err : "unknown";
    sqlite3_free(err);
    throw std::runtime_error("migration failed: " + msg);
  }
}

void ensure_generals_migrations(sqlite3* db) {
  const struct {
    const char* col;
    const char* alter_sql;
  } migrations[] = {
    { "source_text_verbatim", "ALTER TABLE generals ADD COLUMN source_text_verbatim TEXT NOT NULL DEFAULT '';" },
    { "double_checked_in_game", "ALTER TABLE generals ADD COLUMN double_checked_in_game INTEGER NOT NULL DEFAULT 0;" },
    { "country", "ALTER TABLE generals ADD COLUMN country TEXT NOT NULL DEFAULT 'Unknown';" },
    { "has_covenant", "ALTER TABLE generals ADD COLUMN has_covenant INTEGER NOT NULL DEFAULT 0;" },
    { "covenant_member_1", "ALTER TABLE generals ADD COLUMN covenant_member_1 TEXT NOT NULL DEFAULT '';" },
    { "covenant_member_2", "ALTER TABLE generals ADD COLUMN covenant_member_2 TEXT NOT NULL DEFAULT '';" },
    { "covenant_member_3", "ALTER TABLE generals ADD COLUMN covenant_member_3 TEXT NOT NULL DEFAULT '';" },
    { "general_image_blob", "ALTER TABLE generals ADD COLUMN general_image_blob BLOB;" },
    { "general_image_mime", "ALTER TABLE generals ADD COLUMN general_image_mime TEXT NOT NULL DEFAULT '';" },
    { "general_image_filename", "ALTER TABLE generals ADD COLUMN general_image_filename TEXT NOT NULL DEFAULT '';" },
  };

  for (const auto& m : migrations) {
    if (column_exists(db, "generals", m.col)) continue;
    exec_migration_sql(db, m.alter_sql);
  }

  exec_migration_sql(
      db,
      "UPDATE generals SET country='Unknown' "
      "WHERE country IS NULL OR TRIM(country)='' OR "
      "country NOT IN ('Europe','America','Japan','Korea','China','Russia','Arabia','Other','Unknown');");
  exec_migration_sql(
      db,
      "UPDATE generals SET has_covenant=CASE WHEN has_covenant=1 THEN 1 ELSE 0 END;");

  exec_migration_sql(
      db,
      "DROP TRIGGER IF EXISTS trg_generals_role_valid_insert;"
      "DROP TRIGGER IF EXISTS trg_generals_role_valid_update;"
      "DROP TRIGGER IF EXISTS trg_generals_country_valid_insert;"
      "DROP TRIGGER IF EXISTS trg_generals_country_valid_update;"
      "CREATE TRIGGER trg_generals_role_valid_insert "
      "BEFORE INSERT ON generals "
      "FOR EACH ROW "
      "BEGIN "
      "  SELECT CASE "
      "    WHEN NEW.role NOT IN ('Ground','Mounted','Ranged','Siege','Defense','Mixed','Admin','Duty','Mayor','Unknown') "
      "    THEN RAISE(ABORT, 'Invalid generals.role') "
      "  END; "
      "END;"
      "CREATE TRIGGER trg_generals_role_valid_update "
      "BEFORE UPDATE OF role ON generals "
      "FOR EACH ROW "
      "BEGIN "
      "  SELECT CASE "
      "    WHEN NEW.role NOT IN ('Ground','Mounted','Ranged','Siege','Defense','Mixed','Admin','Duty','Mayor','Unknown') "
      "    THEN RAISE(ABORT, 'Invalid generals.role') "
      "  END; "
      "END;"
      "CREATE TRIGGER trg_generals_country_valid_insert "
      "BEFORE INSERT ON generals "
      "FOR EACH ROW "
      "BEGIN "
      "  SELECT CASE "
      "    WHEN NEW.country NOT IN ('Europe','America','Japan','Korea','China','Russia','Arabia','Other','Unknown') "
      "    THEN RAISE(ABORT, 'Invalid generals.country') "
      "  END; "
      "END;"
      "CREATE TRIGGER trg_generals_country_valid_update "
      "BEFORE UPDATE OF country ON generals "
      "FOR EACH ROW "
      "BEGIN "
      "  SELECT CASE "
      "    WHEN NEW.country NOT IN ('Europe','America','Japan','Korea','China','Russia','Arabia','Other','Unknown') "
      "    THEN RAISE(ABORT, 'Invalid generals.country') "
      "  END; "
      "END;");
}

std::string trim_copy(std::string s) {
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  while (!s.empty() && is_space((unsigned char)s.front())) s.erase(s.begin());
  while (!s.empty() && is_space((unsigned char)s.back())) s.pop_back();
  return s;
}

std::string infer_stat_key_kind(const std::string& key) {
  constexpr std::string_view pct_suffix = "Pct";
  constexpr std::string_view flat_suffix = "Flat";
  if (key.size() >= pct_suffix.size() &&
      key.compare(key.size() - pct_suffix.size(), pct_suffix.size(), pct_suffix) == 0) {
    return "percent";
  }
  if (key.size() >= flat_suffix.size() &&
      key.compare(key.size() - flat_suffix.size(), flat_suffix.size(), flat_suffix) == 0) {
    return "flat";
  }
  return "unknown";
}

void sync_canonical_stat_keys(sqlite3* db) {
  const fs::path canonical_path = fs::path("data") / "canonical_keys.txt";
  std::ifstream in(canonical_path);
  if (!in) {
    std::cerr << "warning: could not open canonical key list: " << canonical_path << "\n";
    return;
  }

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "INSERT INTO stat_keys(key, kind, is_active) VALUES(?1, ?2, 1) "
      "ON CONFLICT(key) DO UPDATE SET "
      "  kind=excluded.kind, "
      "  is_active=1;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error("prepare failed: " + std::string(sqlite3_errmsg(db)));
  }

  std::string line;
  while (std::getline(in, line)) {
    std::string key = trim_copy(line);
    if (key.empty()) continue;
    if (!key.empty() && key[0] == '#') continue;

    const std::string kind = infer_stat_key_kind(key);
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, kind.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      const std::string err = sqlite3_errmsg(db);
      sqlite3_finalize(stmt);
      throw std::runtime_error("failed to sync canonical stat key '" + key + "': " + err);
    }
  }

  sqlite3_finalize(stmt);
}

} // namespace

Db::~Db() {
  if (db_) sqlite3_close(db_);
  db_ = nullptr;
}

void Db::open(const std::string& path) {
  if (db_) sqlite3_close(db_);
  db_ = nullptr;
  if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
    std::string err = sqlite3_errmsg(db_);
    sqlite3_close(db_);
    db_ = nullptr;
    throw std::runtime_error("sqlite3_open failed: " + err);
  }
  // Enforce foreign keys.
  exec("PRAGMA foreign_keys=ON;");
  ensure_generals_migrations(db_);
  sync_canonical_stat_keys(db_);
}

void Db::exec(const std::string& sql) {
  char* err = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
    std::string e = err ? err : "unknown";
    sqlite3_free(err);
    throw std::runtime_error("sqlite exec failed: " + e + "\nSQL:\n" + sql);
  }
}

void Db::begin(){ exec("BEGIN;"); }
void Db::commit(){ exec("COMMIT;"); }
void Db::rollback(){ exec("ROLLBACK;"); }

Db::Stmt Db::prepare(const std::string& sql) {
  sqlite3_stmt* s = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &s, nullptr) != SQLITE_OK) {
    throw std::runtime_error("sqlite prepare failed: " + std::string(sqlite3_errmsg(db_)) + "\nSQL:\n" + sql);
  }
  return Stmt(this, s);
}

void Db::Stmt::finalize() {
  if (stmt) sqlite3_finalize(stmt);
  stmt = nullptr;
}
void Db::Stmt::reset() {
  sqlite3_reset(stmt);
  sqlite3_clear_bindings(stmt);
}
bool Db::Stmt::step_row() {
  int rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) return true;
  if (rc == SQLITE_DONE) return false;
  throw std::runtime_error("sqlite step failed: " + std::string(sqlite3_errmsg(owner->db_)));
}
void Db::Stmt::step_done() {
  int rc = sqlite3_step(stmt);
  if (rc == SQLITE_DONE) return;
  throw std::runtime_error("sqlite step_done failed: " + std::string(sqlite3_errmsg(owner->db_)));
}

std::string Db::col_text(sqlite3_stmt* s, int i) {
  auto p = sqlite3_column_text(s, i);
  return p ? std::string(reinterpret_cast<const char*>(p)) : std::string();
}
int Db::col_int(sqlite3_stmt* s, int i) { return sqlite3_column_int(s, i); }
double Db::col_double(sqlite3_stmt* s, int i) { return sqlite3_column_double(s, i); }
bool Db::col_is_null(sqlite3_stmt* s, int i) { return sqlite3_column_type(s, i) == SQLITE_NULL; }

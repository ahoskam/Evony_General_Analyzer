#include "db.h"
#include "db_maintenance.h"

#include <string>

Db::~Db() {
  if (db_) sqlite3_close(db_);
  db_ = nullptr;
}

void Db::open(const std::string& path) {
  if (db_) sqlite3_close(db_);
  db_ = nullptr;
  path_.clear();
  if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
    std::string err = sqlite3_errmsg(db_);
    sqlite3_close(db_);
    db_ = nullptr;
    throw std::runtime_error("sqlite3_open failed: " + err);
  }
  // Enforce foreign keys.
  exec("PRAGMA foreign_keys=ON;");
  std::string err;
  if (!db_apply_generals_migrations(db_, &err)) {
    throw std::runtime_error("migration failed: " + err);
  }
  if (!db_sync_canonical_stat_keys(db_, &err)) {
    throw std::runtime_error("canonical stat key sync failed: " + err);
  }
  path_ = path;
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

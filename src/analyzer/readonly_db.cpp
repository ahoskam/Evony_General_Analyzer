#include "analyzer/readonly_db.h"

#include <utility>

AnalyzerDb::~AnalyzerDb() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

void AnalyzerDb::open_read_only(const std::string& path) {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }

  path_ = path;

  const std::string uri = "file:" + path + "?mode=ro";
  if (sqlite3_open_v2(uri.c_str(), &db_, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI,
                      nullptr) != SQLITE_OK) {
    const std::string err = db_ ? sqlite3_errmsg(db_) : "unknown";
    if (db_) {
      sqlite3_close(db_);
      db_ = nullptr;
    }
    throw std::runtime_error("sqlite3_open_v2 read-only failed: " + err);
  }

  char* err = nullptr;
  if (sqlite3_exec(db_, "PRAGMA query_only=ON;", nullptr, nullptr, &err) !=
      SQLITE_OK) {
    const std::string msg = err ? err : "unknown";
    sqlite3_free(err);
    sqlite3_close(db_);
    db_ = nullptr;
    throw std::runtime_error("failed to enable query_only: " + msg);
  }
}

AnalyzerDb::Stmt::Stmt(Stmt&& other) noexcept
    : stmt(other.stmt), owner(other.owner) {
  other.stmt = nullptr;
  other.owner = nullptr;
}

AnalyzerDb::Stmt& AnalyzerDb::Stmt::operator=(Stmt&& other) noexcept {
  if (this != &other) {
    finalize();
    stmt = other.stmt;
    owner = other.owner;
    other.stmt = nullptr;
    other.owner = nullptr;
  }
  return *this;
}

AnalyzerDb::Stmt::~Stmt() { finalize(); }

void AnalyzerDb::Stmt::finalize() {
  if (stmt) {
    sqlite3_finalize(stmt);
    stmt = nullptr;
  }
}

bool AnalyzerDb::Stmt::step_row() {
  const int rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    return true;
  }
  if (rc == SQLITE_DONE) {
    return false;
  }
  throw std::runtime_error("sqlite step failed: " +
                           std::string(sqlite3_errmsg(owner->db_)));
}

void AnalyzerDb::Stmt::bind_int(int idx, int value) {
  sqlite3_bind_int(stmt, idx, value);
}

void AnalyzerDb::Stmt::bind_text(int idx, const std::string& value) {
  sqlite3_bind_text(stmt, idx, value.c_str(), -1, SQLITE_TRANSIENT);
}

int AnalyzerDb::Stmt::col_int(int idx) const { return sqlite3_column_int(stmt, idx); }

double AnalyzerDb::Stmt::col_double(int idx) const {
  return sqlite3_column_double(stmt, idx);
}

std::string AnalyzerDb::Stmt::col_text(int idx) const {
  const auto* txt = sqlite3_column_text(stmt, idx);
  return txt ? std::string(reinterpret_cast<const char*>(txt)) : std::string();
}

bool AnalyzerDb::Stmt::col_is_null(int idx) const {
  return sqlite3_column_type(stmt, idx) == SQLITE_NULL;
}

AnalyzerDb::Stmt AnalyzerDb::prepare(const std::string& sql) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error("sqlite prepare failed: " +
                             std::string(sqlite3_errmsg(db_)) + "\nSQL:\n" +
                             sql);
  }
  return Stmt(this, stmt);
}

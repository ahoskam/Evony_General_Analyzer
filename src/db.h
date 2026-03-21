#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>

class Db {
public:
  Db() = default;
  ~Db();

  Db(const Db&) = delete;
  Db& operator=(const Db&) = delete;

  void open(const std::string& path);
  bool is_open() const { return db_ != nullptr; }
  sqlite3* raw() { return db_; }
  const std::string& path() const { return path_; }

  void exec(const std::string& sql);
  void begin();
  void commit();
  void rollback();

  struct Stmt {
    sqlite3_stmt* stmt = nullptr;
    Db* owner = nullptr;
    Stmt() = default;
    Stmt(Db* o, sqlite3_stmt* s): stmt(s), owner(o) {}
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;
    Stmt(Stmt&& other) noexcept { stmt = other.stmt; owner = other.owner; other.stmt=nullptr; other.owner=nullptr; }
    Stmt& operator=(Stmt&& other) noexcept {
      if (this != &other) { finalize(); stmt = other.stmt; owner=other.owner; other.stmt=nullptr; other.owner=nullptr; }
      return *this;
    }
    ~Stmt(){ finalize(); }
    void finalize();
    void reset();
    bool step_row(); // true if SQLITE_ROW
    void step_done(); // expects SQLITE_DONE
  };

  Stmt prepare(const std::string& sql);

  static std::string col_text(sqlite3_stmt* s, int i);
  static int col_int(sqlite3_stmt* s, int i);
  static double col_double(sqlite3_stmt* s, int i);
  static bool col_is_null(sqlite3_stmt* s, int i);

private:
  sqlite3* db_ = nullptr;
  std::string path_;
};

// --- Add near the bottom of db.h (after class Db) ---
struct DbStmt {
  Db::Stmt st;

  DbStmt(Db& db, const char* sql) : st(db.prepare(sql)) {}

  void bind_int(int idx, int v) {
    sqlite3_bind_int(st.stmt, idx, v);
  }
  void bind_text(int idx, const char* v) {
    sqlite3_bind_text(st.stmt, idx, v, -1, SQLITE_TRANSIENT);
  }

  // matches your model.cpp usage
  bool step() { return st.step_row(); }

  int col_int(int i) { return Db::col_int(st.stmt, i); }
  double col_double(int i) { return Db::col_double(st.stmt, i); }
  std::string col_text(int i) { return Db::col_text(st.stmt, i); }
  bool col_is_null(int i) { return Db::col_is_null(st.stmt, i); }
};

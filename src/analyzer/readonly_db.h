#pragma once

#include <sqlite3.h>

#include <stdexcept>
#include <string>

class AnalyzerDb {
public:
  AnalyzerDb() = default;
  ~AnalyzerDb();

  AnalyzerDb(const AnalyzerDb&) = delete;
  AnalyzerDb& operator=(const AnalyzerDb&) = delete;

  void open_read_only(const std::string& path);
  bool is_open() const { return db_ != nullptr; }
  sqlite3* raw() { return db_; }
  const std::string& path() const { return path_; }

  struct Stmt {
    sqlite3_stmt* stmt = nullptr;
    AnalyzerDb* owner = nullptr;

    Stmt() = default;
    Stmt(AnalyzerDb* db, sqlite3_stmt* s) : stmt(s), owner(db) {}
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;
    Stmt(Stmt&& other) noexcept;
    Stmt& operator=(Stmt&& other) noexcept;
    ~Stmt();

    void finalize();
    bool step_row();

    void bind_int(int idx, int value);
    void bind_text(int idx, const std::string& value);

    int col_int(int idx) const;
    double col_double(int idx) const;
    std::string col_text(int idx) const;
    bool col_is_null(int idx) const;
  };

  Stmt prepare(const std::string& sql);

private:
  sqlite3* db_ = nullptr;
  std::string path_;
};

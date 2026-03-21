#pragma once

#include <sqlite3.h>
#include <string>

bool db_apply_generals_migrations(sqlite3* db, std::string* err = nullptr);
bool db_sync_canonical_stat_keys(sqlite3* db, std::string* err = nullptr);


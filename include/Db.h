#pragma once
#include <string>
struct sqlite3;

sqlite3* db_open(const std::string& path);
void db_close(sqlite3* db);

// Creates tables (generals, stat_keys, modifiers) + indexes if missing
bool db_ensure_schema(sqlite3* db);

// Syncs stat_keys from StatParse supported keys
bool db_sync_stat_keys(sqlite3* db);

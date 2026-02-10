#pragma once
#include <string>
struct sqlite3;

// Scans import_dir for *.txt. If general name not in DB, imports it.
bool import_generals_from_folder(sqlite3* db, const std::string& import_dir);

#pragma once
#include <string>
#include <vector>

struct sqlite3;
#include "GeneralDefinition.h"

// List all general names in the DB (for later GUI list)
std::vector<std::string> db_list_generals(sqlite3* db);

// Load a general definition (base + ascensions + specialties + covenants) from DB.
// Throws std::runtime_error on failure.
GeneralDefinition db_load_general(sqlite3* db, const std::string& general_name);

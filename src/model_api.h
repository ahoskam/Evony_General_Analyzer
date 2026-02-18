#pragma once
#include "model.h"
#include <vector>
#include <string>

class Db;

// API used by UI
std::vector<GeneralRow> db_load_general_list(Db& db,
    const std::string& role_filter,
    const std::string& name_like);
GeneralMeta db_load_general_meta(Db& db, int general_id);
std::vector<Occurrence> db_load_occurrences(Db& db, int general_id);
std::vector<StatKey> db_load_stat_keys(Db& db);

// Pending
std::vector<PendingExample> db_load_pending_examples_for_general(
  Db& db, const std::string& general_name);


LoadAllResult db_load_all_for_general(Db& db, int general_id);

bool db_update_general_meta(Db& db, int general_id, const GeneralMeta& g);

void db_update_occurrence(Db& db, const Occurrence& o);
int  db_insert_occurrence(Db& db, Occurrence& o);
void db_delete_occurrence(Db& db, int id);

#pragma once

#include "analyzer/compute.h"

#include <map>
#include <string>

struct OwnedStateFile {
  int schema_version = 1;
  std::string db_path_hint;
  std::map<int, OwnedGeneralState> generals;
};

OwnedStateFile load_owned_state_file(const std::string& path);
void save_owned_state_file(const std::string& path, const OwnedStateFile& file);

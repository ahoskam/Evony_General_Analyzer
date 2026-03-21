// src/importer/main_importer.cpp

#include "import_service.h"

#include <iostream>
#include <string>

int main(int argc, char **argv) {
  std::string db_path;
  std::string import_path;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--db" && i + 1 < argc)
      db_path = argv[++i];
    else if (a == "--path" && i + 1 < argc)
      import_path = argv[++i];
  }

  if (db_path.empty() || import_path.empty()) {
    std::cerr << "Usage: importer_v2 --db <dbfile> --path <dir>\n";
    return 1;
  }

  const ImportRunResult result =
      run_import_v2(db_path, import_path, "data/last_import_report.txt");
  if (!result.ok) {
    for (const auto& msg : result.messages) {
      std::cerr << msg << "\n";
    }
    return 1;
  }

  std::cout << "Importer v2 finished.\n"
            << "  Files seen: " << result.files_seen << "\n"
            << "  Generals imported/updated: " << result.generals_imported << "\n"
            << "  Stat occurrences inserted: " << result.occurrences_inserted
            << "\n"
            << "  Pending examples inserted: "
            << result.pending_examples_inserted << "\n"
            << "  Invalid files: " << result.invalid_files << "\n";

  for (const auto& msg : result.messages) {
    std::cerr << msg << "\n";
  }

  return 0;
}

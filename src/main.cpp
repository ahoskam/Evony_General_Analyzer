#include <iostream>

#include "Db.h"
#include "Importer.h"
#include "DbLoad.h"

#include "GeneralLoader.h"
#include "Compute.h"

int main() {
    // 1) Open DB
    sqlite3* db = db_open("data/evony.db");
    if (!db) return 1;

    // 2) Ensure schema exists
    if (!db_ensure_schema(db)) {
        db_close(db);
        return 1;
    }

    // 3) Sync stat keys from code
    if (!db_sync_stat_keys(db)) {
        db_close(db);
        return 1;
    }

    // 4) Import any new generals from data/import/
    if (!import_generals_from_folder(db, "data/import")) {
        db_close(db);
        return 1;
    }

    // ------------------------------------------------------------------
    // For now: keep your existing “load from file and compute” demo
    // Later: we’ll load from DB + drive GUI
    // ------------------------------------------------------------------
    try {
        
        auto def = db_load_general(db, "Lorenzo de' Medici");


        GeneralBuild build;
        build.ascensionLevel = 0;
        build.specialtyLevel = {5,5,5,5};
        build.covenantActive = {1,1,1,1,1,1};

        Stats s = computeStats(def, build);
        s.print_summary(std::cout);
    } catch (const std::exception& e) {
        std::cerr << "Runtime error: " << e.what() << "\n";
    }

    db_close(db);
    return 0;
}

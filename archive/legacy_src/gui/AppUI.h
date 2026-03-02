#pragma once
#include <string>
#include <vector>

struct sqlite3;

#include "GeneralDefinition.h"
#include "GeneralBuild.h"
#include "Stats.h"

struct GeneralListItem {
    int id = -1;
    std::string name;
    std::string role;
};

struct StatKeyItem {
    int id = -1;
    std::string key;
    std::string kind;   // "percent" or "flat"
    std::string label;  // human label (optional)
};

struct ModifierRow {
    int id = -1;
    std::string key;
    double value = 0.0;

    std::string source_type; // base/ascension/specialty/covenant
    int asc = 0;
    int spec_num = 0;
    int spec_lvl = 0;
    int cov_num = 0;
    std::string cov_name;
};

struct AppUI {
    sqlite3* db = nullptr;

    std::vector<GeneralListItem> generals;
    int selectedIndex = -1;

    GeneralDefinition currentDef{};
    GeneralBuild currentBuild{};
    Stats currentStats{};

    bool haveSelection = false;
    bool statsDirty = true;

    // --- Add General modal state
    bool showAddGeneralModal = false;
    std::string addGeneralError;
    char addNameBuf[128] = {0};
    int addRoleIndex = 0;

    // --- Stat keys list (for dropdown)
    std::vector<StatKeyItem> statKeys;

    // --- Add Stat Key modal state
    bool showAddStatKeyModal = false;
    std::string addKeyError;
    char addKeyBuf[128] = {0};
    int addKeyKindIndex = 0; // 0 percent, 1 flat
    char addKeyLabelBuf[128] = {0};

    // --- Add Modifier editor state
    int selectedStatKeyIndex = 0;
    double addValue = 0.0;

    int addSourceType = 0;  // 0 base, 1 ascension, 2 specialty, 3 covenant
    int addAscLevel = 1;    // 1..5
    int addSpecNum  = 1;    // 1..4
    int addSpecLvl  = 1;    // 1..5
    int addCovNum   = 1;    // 1..6
    char addCovNameBuf[128] = {0};

    std::string addModError;

    std::vector<ModifierRow> currentMods;

    // --- Edit General screen state
    int editRoleIndex = 0;          // 0 Ground, 1 Mounted, 2 Ranged, 3 Siege
    int editSelectedIndex = -1;     // index into filtered list

    explicit AppUI(const char* dbPath);
    ~AppUI();

    void draw();

private:
    void loadGeneralList();
    void loadStatKeys();
    void loadModifiersForSelected();

    void selectByIndex(int index);
    void reloadCurrentDefFromDb();
    void recomputeIfDirty();

    // DB helpers
    bool db_insert_general(const std::string& name, const std::string& role, std::string& outError);

    bool db_insert_stat_key(const std::string& key, const std::string& kind, const std::string& label, std::string& outError);

    bool db_insert_modifier(int general_id, int stat_key_id, double value,
                            const std::string& source_type,
                            int asc_level,
                            int spec_num, int spec_lvl,
                            int cov_num, const std::string& cov_name,
                            std::string& outError);
    bool db_delete_modifier(int modifier_id, std::string& outError);

    void drawAddGeneralModal();
    void drawAddStatKeyModal();
    void drawModifierEditor();
    void drawExistingModifiersTable();

    void drawStatsTableFiltered();
};

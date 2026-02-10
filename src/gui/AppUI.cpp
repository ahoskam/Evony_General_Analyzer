#include "AppUI.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <cmath>

#include <imgui.h>
#include <sqlite3.h>

#include "Db.h"
#include "DbLoad.h"
#include "Importer.h"
#include "Compute.h"
#include "Stat.h"

static std::string trim_copy(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace((unsigned char)s[a])) ++a;
    size_t b = s.size();
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

static bool ieq_contains(const std::string& s, const char* needle) {
    std::string ns = s;
    std::string nd = needle;
    std::transform(ns.begin(), ns.end(), ns.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    std::transform(nd.begin(), nd.end(), nd.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return ns.find(nd) != std::string::npos;
}

static bool is_nonzero(double x) {
    return std::fabs(x) > 0.0001;
}

AppUI::AppUI(const char* dbPath) {
    db = db_open(dbPath);
    if (!db) throw std::runtime_error("Failed to open DB");

    if (!db_ensure_schema(db)) throw std::runtime_error("db_ensure_schema() failed");
    if (!db_sync_stat_keys(db)) throw std::runtime_error("db_sync_stat_keys() failed");

    // Keep your two-pronged import (safe/idempotent)
    if (!import_generals_from_folder(db, "data/import")) {
        throw std::runtime_error("import_generals_from_folder() failed");
    }

    loadGeneralList();
    loadStatKeys();

    if (!generals.empty()) {
        selectByIndex(0);
    }
}

AppUI::~AppUI() {
    db_close(db);
    db = nullptr;
}

void AppUI::loadGeneralList() {
    generals.clear();

    const char* sql = R"SQL(
        SELECT id, name, role
        FROM generals
        ORDER BY name;
    )SQL";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        std::cerr << "SQLite prepare failed: " << sqlite3_errmsg(db) << "\n";
        return;
    }

    while (sqlite3_step(st) == SQLITE_ROW) {
        GeneralListItem item;
        item.id = sqlite3_column_int(st, 0);

        const unsigned char* n = sqlite3_column_text(st, 1);
        const unsigned char* r = sqlite3_column_text(st, 2);

        item.name = n ? (const char*)n : "";
        item.role = r ? (const char*)r : "";
        generals.push_back(std::move(item));
    }

    sqlite3_finalize(st);

    if (selectedIndex >= (int)generals.size()) {
        selectedIndex = -1;
        haveSelection = false;
        statsDirty = true;
    }
}

void AppUI::loadStatKeys() {
    statKeys.clear();

    const char* sql = R"SQL(
        SELECT id, key, kind, COALESCE(label,'')
        FROM stat_keys
        ORDER BY key;
    )SQL";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        std::cerr << "SQLite prepare failed: " << sqlite3_errmsg(db) << "\n";
        return;
    }

    while (sqlite3_step(st) == SQLITE_ROW) {
        StatKeyItem k;
        k.id = sqlite3_column_int(st, 0);

        const unsigned char* key = sqlite3_column_text(st, 1);
        const unsigned char* kind = sqlite3_column_text(st, 2);
        const unsigned char* label = sqlite3_column_text(st, 3);

        k.key = key ? (const char*)key : "";
        k.kind = kind ? (const char*)kind : "";
        k.label = label ? (const char*)label : "";

        statKeys.push_back(std::move(k));
    }

    sqlite3_finalize(st);

    if (selectedStatKeyIndex >= (int)statKeys.size()) selectedStatKeyIndex = 0;
}

void AppUI::loadModifiersForSelected() {
    currentMods.clear();
    if (!haveSelection) return;

    const int gid = generals[selectedIndex].id;

    const char* sql = R"SQL(
        SELECT
          m.id,
          sk.key,
          m.value,
          m.source_type,
          COALESCE(m.ascension_level,0),
          COALESCE(m.specialty_number,0),
          COALESCE(m.specialty_level,0),
          COALESCE(m.covenant_number,0),
          COALESCE(m.covenant_name,'')
        FROM modifiers m
        JOIN stat_keys sk ON sk.id = m.stat_key_id
        WHERE m.general_id = ?1
        ORDER BY m.id DESC;
    )SQL";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        std::cerr << "SQLite prepare failed: " << sqlite3_errmsg(db) << "\n";
        return;
    }

    sqlite3_bind_int(st, 1, gid);

    while (sqlite3_step(st) == SQLITE_ROW) {
        ModifierRow row;
        row.id = sqlite3_column_int(st, 0);

        const unsigned char* key = sqlite3_column_text(st, 1);
        row.key = key ? (const char*)key : "";

        row.value = sqlite3_column_double(st, 2);

        const unsigned char* stype = sqlite3_column_text(st, 3);
        row.source_type = stype ? (const char*)stype : "";

        row.asc = sqlite3_column_int(st, 4);
        row.spec_num = sqlite3_column_int(st, 5);
        row.spec_lvl = sqlite3_column_int(st, 6);
        row.cov_num = sqlite3_column_int(st, 7);

        const unsigned char* cn = sqlite3_column_text(st, 8);
        row.cov_name = cn ? (const char*)cn : "";

        currentMods.push_back(std::move(row));
    }

    sqlite3_finalize(st);
}

void AppUI::selectByIndex(int index) {
    if (index < 0 || index >= (int)generals.size()) return;

    selectedIndex = index;
    haveSelection = true;

    reloadCurrentDefFromDb();

    currentBuild = GeneralBuild{};
    currentBuild.ascensionLevel = 0;
    currentBuild.specialtyLevel = {0,0,0,0};
    currentBuild.covenantActive = {0,0,0,0,0,0};

    statsDirty = true;
    recomputeIfDirty();

    loadModifiersForSelected();
}

void AppUI::reloadCurrentDefFromDb() {
    if (selectedIndex < 0 || selectedIndex >= (int)generals.size()) return;
    currentDef = db_load_general(db, generals[selectedIndex].name);
}

void AppUI::recomputeIfDirty() {
    if (!haveSelection || !statsDirty) return;
    currentStats = computeStats(currentDef, currentBuild);
    statsDirty = false;
}

bool AppUI::db_insert_general(const std::string& name, const std::string& role, std::string& outError) {
    outError.clear();
    const char* sql = "INSERT INTO generals(name, role) VALUES (?1, ?2);";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        outError = std::string("SQLite prepare failed: ") + sqlite3_errmsg(db);
        return false;
    }

    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, role.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(st);
    sqlite3_finalize(st);

    if (rc != SQLITE_DONE) {
        outError = std::string("SQLite insert failed: ") + sqlite3_errmsg(db);
        return false;
    }
    return true;
}

bool AppUI::db_insert_stat_key(const std::string& key, const std::string& kind, const std::string& label, std::string& outError) {
    outError.clear();

    const char* sql = R"SQL(
      INSERT INTO stat_keys(key, kind, label)
      VALUES (?1, ?2, ?3);
    )SQL";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        outError = std::string("SQLite prepare failed: ") + sqlite3_errmsg(db);
        return false;
    }

    sqlite3_bind_text(st, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, kind.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, label.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(st);
    sqlite3_finalize(st);

    if (rc != SQLITE_DONE) {
        outError = std::string("SQLite insert failed: ") + sqlite3_errmsg(db);
        return false;
    }
    return true;
}

bool AppUI::db_insert_modifier(
    int general_id, int stat_key_id, double value,
    const std::string& source_type,
    int asc_level,
    int spec_num, int spec_lvl,
    int cov_num, const std::string& cov_name,
    std::string& outError)
{
    outError.clear();

    const char* sql = R"SQL(
      INSERT INTO modifiers(
        general_id, stat_key_id, source_type,
        ascension_level, specialty_number, specialty_level,
        covenant_number, covenant_name,
        value, raw_text
      )
      VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, NULL);
    )SQL";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        outError = std::string("SQLite prepare failed: ") + sqlite3_errmsg(db);
        return false;
    }

    sqlite3_bind_int(st, 1, general_id);
    sqlite3_bind_int(st, 2, stat_key_id);
    sqlite3_bind_text(st, 3, source_type.c_str(), -1, SQLITE_TRANSIENT);

    if (source_type == "ascension") sqlite3_bind_int(st, 4, asc_level);
    else sqlite3_bind_null(st, 4);

    if (source_type == "specialty") {
        sqlite3_bind_int(st, 5, spec_num);
        sqlite3_bind_int(st, 6, spec_lvl);
    } else {
        sqlite3_bind_null(st, 5);
        sqlite3_bind_null(st, 6);
    }

    if (source_type == "covenant") {
        sqlite3_bind_int(st, 7, cov_num);
        sqlite3_bind_text(st, 8, cov_name.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(st, 7);
        sqlite3_bind_null(st, 8);
    }

    sqlite3_bind_double(st, 9, value);

    int rc = sqlite3_step(st);
    sqlite3_finalize(st);

    if (rc != SQLITE_DONE) {
        outError = std::string("SQLite insert failed: ") + sqlite3_errmsg(db);
        return false;
    }
    return true;
}

bool AppUI::db_delete_modifier(int modifier_id, std::string& outError) {
    outError.clear();

    const char* sql = "DELETE FROM modifiers WHERE id=?1;";
    sqlite3_stmt* st = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        outError = std::string("SQLite prepare failed: ") + sqlite3_errmsg(db);
        return false;
    }

    sqlite3_bind_int(st, 1, modifier_id);

    int rc = sqlite3_step(st);
    sqlite3_finalize(st);

    if (rc != SQLITE_DONE) {
        outError = std::string("SQLite delete failed: ") + sqlite3_errmsg(db);
        return false;
    }
    return true;
}

void AppUI::drawAddGeneralModal() {
    if (showAddGeneralModal) {
        ImGui::OpenPopup("Add General");
        showAddGeneralModal = false;
    }

    static const char* roles[] = {"Ground", "Mounted", "Ranged", "Siege", "Assistant", "Other"};

    if (ImGui::BeginPopupModal("Add General", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Create a new general (name + role).");
        ImGui::Spacing();

        ImGui::InputText("Name", addNameBuf, sizeof(addNameBuf));
        ImGui::Combo("Role", &addRoleIndex, roles, (int)(sizeof(roles)/sizeof(roles[0])));

        if (!addGeneralError.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1,0.35f,0.35f,1), "%s", addGeneralError.c_str());
        }

        ImGui::Spacing();
        bool doCreate = ImGui::Button("Create");
        ImGui::SameLine();
        bool doCancel = ImGui::Button("Cancel");

        if (doCancel) {
            addGeneralError.clear();
            addNameBuf[0] = '\0';
            addRoleIndex = 0;
            ImGui::CloseCurrentPopup();
        }

        if (doCreate) {
            std::string name = trim_copy(std::string(addNameBuf));
            std::string role = roles[std::clamp(addRoleIndex, 0, (int)(sizeof(roles)/sizeof(roles[0])) - 1)];

            if (name.empty()) {
                addGeneralError = "Name cannot be empty.";
            } else {
                std::string err;
                if (!db_insert_general(name, role, err)) {
                    if (err.find("UNIQUE") != std::string::npos) addGeneralError = "A general with that name already exists.";
                    else addGeneralError = err;
                } else {
                    loadGeneralList();
                    for (int i = 0; i < (int)generals.size(); ++i) {
                        if (generals[i].name == name) {
                            selectByIndex(i);
                            break;
                        }
                    }
                    addGeneralError.clear();
                    addNameBuf[0] = '\0';
                    addRoleIndex = 0;
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndPopup();
    }
}

void AppUI::drawAddStatKeyModal() {
    if (showAddStatKeyModal) {
        ImGui::OpenPopup("Add Stat Key");
        showAddStatKeyModal = false;
    }

    static const char* kinds[] = {"percent", "flat"};

    if (ImGui::BeginPopupModal("Add Stat Key", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Add a stat key into the database.");
        ImGui::TextDisabled("Note: it will NOT compute until you map it in StatParse.cpp.");
        ImGui::Spacing();

        ImGui::InputText("Key", addKeyBuf, sizeof(addKeyBuf));
        ImGui::Combo("Kind", &addKeyKindIndex, kinds, 2);
        ImGui::InputText("Label (optional)", addKeyLabelBuf, sizeof(addKeyLabelBuf));

        if (!addKeyError.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1,0.35f,0.35f,1), "%s", addKeyError.c_str());
        }

        ImGui::Spacing();
        bool doAdd = ImGui::Button("Add");
        ImGui::SameLine();
        bool doCancel = ImGui::Button("Cancel");

        if (doCancel) {
            addKeyError.clear();
            addKeyBuf[0] = '\0';
            addKeyLabelBuf[0] = '\0';
            addKeyKindIndex = 0;
            ImGui::CloseCurrentPopup();
        }

        if (doAdd) {
            addKeyError.clear();

            std::string key = trim_copy(std::string(addKeyBuf));
            std::string kind = kinds[std::clamp(addKeyKindIndex, 0, 1)];
            std::string label = trim_copy(std::string(addKeyLabelBuf));

            if (key.empty()) {
                addKeyError = "Key cannot be empty.";
            } else {
                std::string err;
                if (!db_insert_stat_key(key, kind, label, err)) {
                    if (err.find("UNIQUE") != std::string::npos) addKeyError = "That key already exists.";
                    else addKeyError = err;
                } else {
                    loadStatKeys();
                    addKeyBuf[0] = '\0';
                    addKeyLabelBuf[0] = '\0';
                    addKeyKindIndex = 0;
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndPopup();
    }
}

void AppUI::drawModifierEditor() {
    if (!haveSelection) return;
    if (statKeys.empty()) {
        ImGui::TextColored(ImVec4(1,0.35f,0.35f,1), "No stat_keys loaded (DB empty?)");
        return;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Add Modifier");
    ImGui::Spacing();

    if (ImGui::Button("Add Stat Key...")) {
        showAddStatKeyModal = true;
    }
    drawAddStatKeyModal();

    static const char* sources[] = {"Base", "Ascension", "Specialty", "Covenant"};
    ImGui::Combo("Source", &addSourceType, sources, 4);

    // build labels "key (kind)"
    std::vector<std::string> labels;
    labels.reserve(statKeys.size());
    for (auto& k : statKeys) labels.push_back(k.key + " (" + k.kind + ")");

    std::vector<const char*> labelPtrs;
    labelPtrs.reserve(labels.size());
    for (auto& s : labels) labelPtrs.push_back(s.c_str());

    if (selectedStatKeyIndex >= (int)statKeys.size()) selectedStatKeyIndex = 0;
    ImGui::Combo("Stat Key", &selectedStatKeyIndex, labelPtrs.data(), (int)labelPtrs.size());

    ImGui::InputDouble("Value", &addValue, 1.0, 10.0, "%.2f");

    if (addSourceType == 1) {
        ImGui::SliderInt("Ascension Level", &addAscLevel, 1, 5);
    } else if (addSourceType == 2) {
        ImGui::SliderInt("Specialty #", &addSpecNum, 1, 4);
        ImGui::SliderInt("Specialty Level", &addSpecLvl, 1, 5);
    } else if (addSourceType == 3) {
        ImGui::SliderInt("Covenant #", &addCovNum, 1, 6);
        ImGui::InputText("Covenant Name (optional)", addCovNameBuf, sizeof(addCovNameBuf));
    }

    if (!addModError.empty()) {
        ImGui::TextColored(ImVec4(1,0.35f,0.35f,1), "%s", addModError.c_str());
    }

    if (ImGui::Button("Add Modifier")) {
        addModError.clear();

        const int general_id = generals[selectedIndex].id;
        const auto& sk = statKeys[selectedStatKeyIndex];

        std::string source_type;
        int asc = 0, sn = 0, sl = 0, cn = 0;
        std::string cname;

        if (addSourceType == 0) source_type = "base";
        else if (addSourceType == 1) { source_type = "ascension"; asc = addAscLevel; }
        else if (addSourceType == 2) { source_type = "specialty"; sn = addSpecNum; sl = addSpecLvl; }
        else { source_type = "covenant"; cn = addCovNum; cname = trim_copy(std::string(addCovNameBuf)); }

        std::string err;
        if (!db_insert_modifier(general_id, sk.id, addValue, source_type, asc, sn, sl, cn, cname, err)) {
            addModError = err;
        } else {
            reloadCurrentDefFromDb();
            statsDirty = true;
            recomputeIfDirty();
            loadModifiersForSelected();
            addValue = 0.0;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Reload from DB")) {
        reloadCurrentDefFromDb();
        statsDirty = true;
        recomputeIfDirty();
        loadModifiersForSelected();
    }
}

void AppUI::drawExistingModifiersTable() {
    if (!haveSelection) return;

    ImGui::Separator();
    ImGui::TextUnformatted("Existing Modifiers (selected general)");
    ImGui::TextDisabled("If totals look low, your StatParse.cpp may not map some keys yet.");

    if (ImGui::BeginTable("mods_table", 6,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
        ImVec2(0, 220))) {

        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Key");
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("Context");
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();

        for (const auto& m : currentMods) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", m.id);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(m.key.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", m.value);

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(m.source_type.c_str());

            ImGui::TableSetColumnIndex(4);
            std::string ctx;
            if (m.source_type == "ascension") ctx = "L" + std::to_string(m.asc);
            else if (m.source_type == "specialty") ctx = "S" + std::to_string(m.spec_num) + " L" + std::to_string(m.spec_lvl);
            else if (m.source_type == "covenant") {
                ctx = "#" + std::to_string(m.cov_num);
                if (!m.cov_name.empty()) ctx += " " + m.cov_name;
            } else ctx = "-";
            ImGui::TextUnformatted(ctx.c_str());

            ImGui::TableSetColumnIndex(5);
            std::string btn = "Delete##" + std::to_string(m.id);
            if (ImGui::SmallButton(btn.c_str())) {
                std::string err;
                if (!db_delete_modifier(m.id, err)) {
                    std::cerr << "Delete failed: " << err << "\n";
                } else {
                    reloadCurrentDefFromDb();
                    statsDirty = true;
                    recomputeIfDirty();
                    loadModifiersForSelected();
                }
            }
        }

        ImGui::EndTable();
    }
}

void AppUI::drawStatsTableFiltered() {
    // Determine primary troop type from role string (we'll still always show the primary triplet even if zero)
    enum class Primary { Ground, Mounted, Ranged, Siege, Other };
    Primary p = Primary::Other;

    if (ieq_contains(currentDef.role, "ground"))  p = Primary::Ground;
    else if (ieq_contains(currentDef.role, "mounted")) p = Primary::Mounted;
    else if (ieq_contains(currentDef.role, "ranged"))  p = Primary::Ranged;
    else if (ieq_contains(currentDef.role, "siege"))   p = Primary::Siege;

    auto is_primary_triplet = [&](Stat s) {
        if (p == Primary::Ground)  return s == Stat::GroundAttackPct || s == Stat::GroundDefensePct || s == Stat::GroundHPPct;
        if (p == Primary::Mounted) return s == Stat::MountedAttackPct || s == Stat::MountedDefensePct || s == Stat::MountedHPPct;
        if (p == Primary::Ranged)  return s == Stat::RangedAttackPct || s == Stat::RangedDefensePct || s == Stat::RangedHPPct;
        if (p == Primary::Siege)   return s == Stat::SiegeAttackPct || s == Stat::SiegeDefensePct || s == Stat::SiegeHPPct;
        return false;
    };

    auto is_base_troop_stat = [&](Stat s) {
        return s == Stat::GroundAttackPct || s == Stat::GroundDefensePct || s == Stat::GroundHPPct ||
               s == Stat::MountedAttackPct || s == Stat::MountedDefensePct || s == Stat::MountedHPPct ||
               s == Stat::RangedAttackPct || s == Stat::RangedDefensePct || s == Stat::RangedHPPct ||
               s == Stat::SiegeAttackPct || s == Stat::SiegeDefensePct || s == Stat::SiegeHPPct;
    };

    struct RowPct {
        const char* name;
        Stat stat;
    };
    struct RowFlat {
        const char* name;
        Stat stat;
    };

    // What we want to show: keep it tight and useful.
    // Add/remove rows here as you expand the project.
    const RowPct pctRows[] = {
        // Troop base stats (shown as EFFECTIVE)
        {"Ground ATK",  Stat::GroundAttackPct},
        {"Ground DEF",  Stat::GroundDefensePct},
        {"Ground HP",   Stat::GroundHPPct},

        {"Mounted ATK", Stat::MountedAttackPct},
        {"Mounted DEF", Stat::MountedDefensePct},
        {"Mounted HP",  Stat::MountedHPPct},

        {"Ranged ATK",  Stat::RangedAttackPct},
        {"Ranged DEF",  Stat::RangedDefensePct},
        {"Ranged HP",   Stat::RangedHPPct},

        {"Siege ATK",   Stat::SiegeAttackPct},
        {"Siege DEF",   Stat::SiegeDefensePct},
        {"Siege HP",    Stat::SiegeHPPct},

        // Common buffs
        {"March Speed", Stat::MarchSpeedPct},
        {"March Speed to Monster", Stat::MarchSpeedToMonsterPct},

        {"Attack vs Enemy",  Stat::AttackVsEnemyPct},
        {"Defense vs Enemy", Stat::DefenseVsEnemyPct},
        {"HP vs Enemy",      Stat::HPVsEnemyPct},

        {"Attack vs Monsters",  Stat::AttackAgainstMonstersPct},
        {"Defense vs Monsters", Stat::DefenseAgainstMonstersPct},
        {"HP vs Monsters",      Stat::HPAgainstMonstersPct},

        {"Construction Speed", Stat::ConstructionSpeedPct},
        {"Research Speed",     Stat::ResearchSpeedPct},
        {"Training Speed",     Stat::TrainingSpeedPct},
        {"City Defense",       Stat::CityDefensePct},

        {"Food Gathering Speed",   Stat::FoodGatheringSpeedPct},
        {"Lumber Gathering Speed", Stat::LumberGatheringSpeedPct},
        {"Stone Gathering Speed",  Stat::StoneGatheringSpeedPct},
        {"Ore Gathering Speed",    Stat::OreGatheringSpeedPct},

        {"Food Production",   Stat::FoodProductionPct},
        {"Lumber Production", Stat::LumberProductionPct},
        {"Stone Production",  Stat::StoneProductionPct},
        {"Ore Production",    Stat::OreProductionPct},

        {"Reduce Damage Taken",       Stat::ReduceDamageTakenPct},
        {"Increase Damage Dealt",     Stat::IncreaseDamageDealtPct},
        {"Increase Damage to Enemy",  Stat::IncreaseDamageToEnemyPct},
        {"Increase Damage to Monsters", Stat::IncreaseDamageToMonstersPct},
        {"Reduce Damage from Enemy",  Stat::ReduceDamageFromEnemyPct},
        {"Reduce Damage from Monsters", Stat::ReduceDamageFromMonstersPct},

        {"Rally Size", Stat::RallySizePct},
        {"Sub-City Construction Speed", Stat::SubCityConstructionSpeedPct},
        {"Sub-City Training Speed",     Stat::SubCityTrainingSpeedPct},
    };

    const RowFlat flatRows[] = {
        {"March Size Flat",  Stat::MarchSizeFlat},
        {"Rally Capacity",   Stat::RallyCapacityFlat},
        {"Free Research Time", Stat::FreeResearchTimeFlat},
    };

    if (ImGui::BeginTable("stats_table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Stat");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        // Percent rows
        for (const auto& r : pctRows) {
            double v = 0.0;

            if (is_base_troop_stat(r.stat)) {
                // EFFECTIVE view for troop stats (base + attacking-only)
                v = currentStats.effective_percent(r.stat);
            } else {
                // raw view for everything else
                v = currentStats.total_percent(r.stat);
            }

            // Show only nonzero, EXCEPT always show primary triplet (even if zero)
            if (!is_primary_triplet(r.stat) && !is_nonzero(v)) continue;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(r.name);

            ImGui::TableSetColumnIndex(1);

            // For troop stats, show breakdown inline if the attacking-only portion is nonzero
            if (is_base_troop_stat(r.stat)) {
                double base = currentStats.total_percent(r.stat);
                double atk_only = v - base;

                if (is_nonzero(atk_only)) {
                    ImGui::Text("%.1f%%  (base %.1f + atk %.1f)", v, base, atk_only);
                } else {
                    ImGui::Text("%.1f%%", v);
                }
            } else {
                ImGui::Text("%.1f%%", v);
            }
        }

        // Flat rows (only show nonzero)
        for (const auto& r : flatRows) {
            double v = currentStats.total_flat(r.stat);
            if (!is_nonzero(v)) continue;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(r.name);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("+%.0f", v);
        }

        // March Size % is useful too (percent version)
        {
            double v = currentStats.total_percent(Stat::MarchSizePct);
            if (is_nonzero(v)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("March Size %");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f%%", v);
            }
        }

        ImGui::EndTable();
    }
}

void AppUI::draw() {
    ImGui::Begin("Evony Generals");

    if (ImGui::Button("Add General...")) showAddGeneralModal = true;
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        loadGeneralList();
        loadStatKeys();
        if (haveSelection) loadModifiersForSelected();
    }

    drawAddGeneralModal();

    ImGui::Separator();
    ImGui::Columns(2, nullptr, true);
    ImGui::SetColumnWidth(0, 320);

    // LEFT
    ImGui::TextUnformatted("Generals");
    ImGui::Separator();
    ImGui::BeginChild("general_list", ImVec2(0, 0), true);
    for (int i = 0; i < (int)generals.size(); ++i) {
        const bool isSel = (selectedIndex == i);
        if (ImGui::Selectable(generals[i].name.c_str(), isSel)) {
            selectByIndex(i);
        }
    }
    ImGui::EndChild();

    ImGui::NextColumn();

    // RIGHT
    ImGui::TextUnformatted("Build");
    ImGui::Separator();

    if (!haveSelection) {
        ImGui::TextDisabled("Select a general on the left.");
        ImGui::Columns(1);
        ImGui::End();
        return;
    }

    ImGui::Text("GENERAL: %s", currentDef.name.c_str());
    ImGui::Text("ROLE: %s", currentDef.role.c_str());

    ImGui::Separator();

    bool changed = false;
    changed |= ImGui::SliderInt("Ascension", &currentBuild.ascensionLevel, 0, 5);
    for (int i = 0; i < 4; ++i) {
        std::string label = "Specialty " + std::to_string(i + 1);
        changed |= ImGui::SliderInt(label.c_str(), &currentBuild.specialtyLevel[i], 0, 5);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Covenants");
    for (int i = 0; i < 6; ++i) {
        bool enabled = (currentBuild.covenantActive[i] != 0);
        std::string label = "Covenant " + std::to_string(i + 1);
        if (ImGui::Checkbox(label.c_str(), &enabled)) {
            currentBuild.covenantActive[i] = enabled ? 1 : 0;
            changed = true;
        }
    }

    if (changed) statsDirty = true;
    recomputeIfDirty();

    ImGui::Separator();
    ImGui::TextUnformatted("Computed Stats (filtered)");
    drawStatsTableFiltered();

    drawModifierEditor();
    drawExistingModifiersTable();

    ImGui::Columns(1);
    ImGui::End();
}

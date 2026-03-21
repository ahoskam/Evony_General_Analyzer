#include "analyzer/ui.h"

#include "analyzer/compute.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <optional>

namespace {

struct RoleGroup {
  const char* role;
  const char* label;
};

constexpr RoleGroup kRoleGroups[] = {
    {"Ground", "Ground"},
    {"Mounted", "Mounted"},
    {"Ranged", "Ranged"},
    {"Siege", "Siege"},
    {"Defense", "Defense"},
    {"Mixed", "Mixed"},
    {"Admin", "Admin"},
    {"Duty", "Duty"},
    {"Mayor", "Mayor"},
    {"Unknown", "Unknown"},
};

bool contains_case_insensitive(const std::string& text,
                               const std::string& needle) {
  if (needle.empty()) {
    return true;
  }

  auto lower = [](std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });
    return s;
  };

  return lower(text).find(lower(needle)) != std::string::npos;
}

const OwnedGeneralState* find_owned_state(const AnalyzerAppState& state,
                                          int general_id) {
  auto it = state.owned_file.generals.find(general_id);
  return it == state.owned_file.generals.end() ? nullptr : &it->second;
}

OwnedGeneralState default_owned_state_for_general(
    const AnalyzerGeneralData& general) {
  OwnedGeneralState owned;
  owned.general_id = general.id;
  owned.general_name = general.name;
  owned.general_level = 1;
  return owned;
}

void load_selected_general(AnalyzerDb& db, AnalyzerAppState& state,
                           int general_id) {
  state.selected_general = analyzer_load_general_data(db, general_id);
  state.selected_general_id = general_id;
  state.has_loaded_selected = true;

  if (const OwnedGeneralState* existing =
          find_owned_state(state, state.selected_general.id)) {
    state.selected_owned = *existing;
    state.selected_is_owned = true;
  } else {
    state.selected_owned = default_owned_state_for_general(state.selected_general);
    state.selected_is_owned = false;
  }
}

void save_state(AnalyzerAppState& state) {
  state.owned_file.schema_version = 1;
  save_owned_state_file(state.state_path, state.owned_file);
  state.dirty = false;
  state.status_message = "Owned state saved to " + state.state_path;
}

void load_state_from_path(AnalyzerAppState& state) {
  state.owned_file = load_owned_state_file(state.state_path);
  state.owned_file.db_path_hint = state.owned_file.db_path_hint.empty()
                                      ? "data/evony_v2.db"
                                      : state.owned_file.db_path_hint;
  state.dirty = false;
  state.status_message = "Loaded state file: " + state.state_path;
}

void create_blank_state_at_path(AnalyzerAppState& state) {
  state.owned_file = OwnedStateFile{};
  state.owned_file.schema_version = 1;
  save_owned_state_file(state.state_path, state.owned_file);
  state.dirty = false;
  state.status_message = "Created blank state file: " + state.state_path;
}

void sync_selected_owned_to_file(AnalyzerAppState& state) {
  if (!state.has_loaded_selected) {
    return;
  }
  if (state.selected_is_owned) {
    state.selected_owned.general_id = state.selected_general.id;
    state.selected_owned.general_name = state.selected_general.name;
    state.owned_file.generals[state.selected_general.id] = state.selected_owned;
  } else {
    state.owned_file.generals.erase(state.selected_general.id);
  }
}

ImVec4 general_name_color(const AnalyzerAppState& state, int general_id) {
  const OwnedGeneralState* owned = find_owned_state(state, general_id);
  if (!owned) {
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
  }

  switch (std::clamp(owned->ascension_level, 0, 5)) {
    case 0:
      return ImVec4(0.35f, 0.85f, 0.35f, 1.0f);
    case 1:
      return ImVec4(0.70f, 0.45f, 0.95f, 1.0f);
    case 2:
      return ImVec4(0.35f, 0.60f, 1.0f, 1.0f);
    case 3:
      return ImVec4(0.95f, 0.85f, 0.25f, 1.0f);
    case 4:
      return ImVec4(1.0f, 0.58f, 0.20f, 1.0f);
    case 5:
      return ImVec4(0.95f, 0.25f, 0.25f, 1.0f);
    default:
      return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
  }
}

void draw_status_badge(const char* text, const ImVec4& color) {
  ImGui::PushStyleColor(ImGuiCol_Button, color);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
  ImGui::Button(text);
  ImGui::PopStyleColor(4);
}

void draw_general_entry(AnalyzerDb& db, AnalyzerAppState& state,
                        const AnalyzerGeneralListItem& item) {
  const bool selected = state.selected_general_id == item.id;
  char label[256];
  std::snprintf(label, sizeof(label), "%s##general_%d", item.name.c_str(),
                item.id);

  ImGui::PushStyleColor(ImGuiCol_Text, general_name_color(state, item.id));
  if (ImGui::Selectable(label, selected)) {
    load_selected_general(db, state, item.id);
  }
  ImGui::PopStyleColor();

  if (ImGui::IsItemHovered()) {
    const OwnedGeneralState* owned = find_owned_state(state, item.id);
    const int ascension_level = owned ? owned->ascension_level : -1;
    if (ascension_level >= 0) {
      ImGui::SetTooltip("%s%s | Owned | Ascension %d", item.role.c_str(),
                        item.has_covenant ? " | Covenant" : "",
                        std::clamp(ascension_level, 0, 5));
    } else {
      ImGui::SetTooltip("%s%s | Unowned", item.role.c_str(),
                        item.has_covenant ? " | Covenant" : "");
    }
  }
}

void draw_general_list(AnalyzerDb& db, AnalyzerAppState& state) {
  ImGui::BeginChild("general_list", ImVec2(320, 0), true);
  ImGui::TextUnformatted("Checked Generals");
  ImGui::InputText("Search", &state.search_text);
  ImGui::Separator();

  std::map<std::string, std::vector<const AnalyzerGeneralListItem*>> grouped;
  for (const auto& item : state.general_list) {
    if (contains_case_insensitive(item.name, state.search_text) ||
        contains_case_insensitive(item.role, state.search_text)) {
      grouped[item.role].push_back(&item);
    }
  }

  for (const auto& group : kRoleGroups) {
    auto it = grouped.find(group.role);
    if (it == grouped.end() || it->second.empty()) {
      continue;
    }

    char header[128];
    std::snprintf(header, sizeof(header), "%s (%zu)", group.label,
                  it->second.size());
    if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
      for (const AnalyzerGeneralListItem* item : it->second) {
        draw_general_entry(db, state, *item);
      }
    }
  }

  for (const auto& [role, items] : grouped) {
    const bool known_role = std::any_of(
        std::begin(kRoleGroups), std::end(kRoleGroups),
        [&](const RoleGroup& group) { return role == group.role; });
    if (known_role || items.empty()) {
      continue;
    }

    char header[128];
    std::snprintf(header, sizeof(header), "%s (%zu)", role.c_str(),
                  items.size());
    if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
      for (const AnalyzerGeneralListItem* item : items) {
        draw_general_entry(db, state, *item);
      }
    }
  }

  ImGui::EndChild();
}

void draw_totals_table(const std::map<std::string, double>& totals) {
  if (!ImGui::BeginTable("totals", 2,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_SizingStretchProp)) {
    return;
  }

  ImGui::TableSetupColumn("Stat Key");
  ImGui::TableSetupColumn("Value");
  ImGui::TableHeadersRow();

  for (const auto& [key, value] : totals) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(key.c_str());
    ImGui::TableNextColumn();
    ImGui::Text("%.2f", value);
  }

  ImGui::EndTable();
}

struct CombinedSummaryKeys {
  const char* regular_attack = nullptr;
  const char* regular_defense = nullptr;
  const char* regular_hp = nullptr;
  const char* attacking_attack = nullptr;
  const char* attacking_defense = nullptr;
  const char* attacking_hp = nullptr;
  const char* dragon_attack = nullptr;
  const char* dragon_defense = nullptr;
  const char* dragon_hp = nullptr;
  const char* beast_attack = nullptr;
  const char* beast_defense = nullptr;
  const char* beast_hp = nullptr;
};

std::optional<CombinedSummaryKeys> summary_keys_for_role(const std::string& role) {
  if (role == "Ground") {
    return CombinedSummaryKeys{
        "GroundTroopAttackPct", "GroundTroopDefensePct", "GroundTroopHPPct",
        "AttackingGroundAttackPct", "AttackingGroundDefensePct",
        "AttackingGroundHPPct", "AttackingWithDragonGroundTroopAttackPct",
        "AttackingWithDragonGroundTroopDefensePct",
        "AttackingWithDragonGroundTroopHPPct",
        "AttackingWithBeastGroundTroopAttackPct",
        "AttackingWithBeastGroundTroopDefensePct",
        "AttackingWithBeastGroundTroopHPPct"};
  }
  if (role == "Mounted") {
    return CombinedSummaryKeys{
        "MountedTroopAttackPct", "MountedTroopDefensePct", "MountedTroopHPPct",
        "AttackingMountedAttackPct", "AttackingMountedDefensePct",
        "AttackingMountedHPPct", "AttackingWithDragonMountedTroopAttackPct",
        "AttackingWithDragonMountedTroopDefensePct",
        "AttackingWithDragonMountedTroopHPPct",
        "AttackingWithBeastMountedTroopAttackPct",
        "AttackingWithBeastMountedTroopDefensePct",
        "AttackingWithBeastMountedTroopHPPct"};
  }
  if (role == "Ranged") {
    return CombinedSummaryKeys{
        "RangedTroopAttackPct", "RangedTroopDefensePct", "RangedTroopHPPct",
        "AttackingRangedAttackPct", "AttackingRangedDefensePct",
        "AttackingRangedHPPct", "AttackingWithDragonRangedTroopAttackPct",
        "AttackingWithDragonRangedTroopDefensePct",
        "AttackingWithDragonRangedTroopHPPct",
        "AttackingWithBeastRangedTroopAttackPct",
        "AttackingWithBeastRangedTroopDefensePct",
        "AttackingWithBeastRangedTroopHPPct"};
  }
  if (role == "Siege") {
    return CombinedSummaryKeys{
        "SiegeMachineAttackPct", "SiegeMachineDefensePct", "SiegeMachineHPPct",
        "AttackingSiegeAttackPct", "AttackingSiegeDefensePct",
        "AttackingSiegeHPPct", "AttackingWithDragonSiegeTroopAttackPct",
        "AttackingWithDragonSiegeTroopDefensePct",
        "AttackingWithDragonSiegeTroopHPPct",
        "AttackingWithBeastSiegeTroopAttackPct",
        "AttackingWithBeastSiegeTroopDefensePct",
        "AttackingWithBeastSiegeTroopHPPct"};
  }
  return std::nullopt;
}

double value_or_zero(const std::map<std::string, double>& totals, const char* key) {
  auto it = totals.find(key);
  return it == totals.end() ? 0.0 : it->second;
}

void draw_combined_summary_rows(const char* table_id,
                                const char* label_prefix,
                                const AnalyzerGeneralData& general,
                                const OwnedGeneralState& owned,
                                const std::map<std::string, double>& totals) {
  const auto summary = summary_keys_for_role(general.role);
  if (!summary.has_value()) {
    return;
  }

  const double total_attack =
      value_or_zero(totals, summary->regular_attack) +
      value_or_zero(totals, summary->attacking_attack) +
      (owned.has_dragon ? value_or_zero(totals, summary->dragon_attack) : 0.0) +
      (owned.has_spirit_beast ? value_or_zero(totals, summary->beast_attack)
                              : 0.0);
  const double total_defense =
      value_or_zero(totals, summary->regular_defense) +
      value_or_zero(totals, summary->attacking_defense) +
      (owned.has_dragon ? value_or_zero(totals, summary->dragon_defense) : 0.0) +
      (owned.has_spirit_beast ? value_or_zero(totals, summary->beast_defense)
                              : 0.0);
  const double total_hp =
      value_or_zero(totals, summary->regular_hp) +
      value_or_zero(totals, summary->attacking_hp) +
      (owned.has_dragon ? value_or_zero(totals, summary->dragon_hp) : 0.0) +
      (owned.has_spirit_beast ? value_or_zero(totals, summary->beast_hp)
                              : 0.0);
  const double total_march_size = value_or_zero(totals, "MarchSizePct");

  ImGui::Spacing();
  if (ImGui::BeginTable(table_id, 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    const ImU32 text = IM_COL32(255, 245, 245, 255);

    auto draw_row = [&](const char* label, double value, ImU32 bg) {
      ImGui::TableNextRow();
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, bg);
      ImGui::TableNextColumn();
      ImGui::TextColored(ImColor(text), "%s", label);
      ImGui::TableNextColumn();
      ImGui::TextColored(ImColor(text), "%.2f", value);
    };

    const std::string attack_label =
        std::string(label_prefix) + "Attacking Attack";
    const std::string defense_label =
        std::string(label_prefix) + "Attacking Defense";
    const std::string hp_label = std::string(label_prefix) + "Attacking HP";
    const std::string march_size_label =
        std::string(label_prefix) + "March Size";

    draw_row(attack_label.c_str(), total_attack, IM_COL32(150, 36, 36, 255));
    draw_row(defense_label.c_str(), total_defense, IM_COL32(36, 72, 150, 255));
    draw_row(hp_label.c_str(), total_hp, IM_COL32(84, 36, 150, 255));
    draw_row(march_size_label.c_str(), total_march_size,
             IM_COL32(34, 120, 52, 255));
    ImGui::EndTable();
  }
}

}  // namespace

void analyzer_ui_tick(AnalyzerDb& db, AnalyzerAppState& state) {
  if (state.general_list.empty()) {
    state.general_list = analyzer_load_general_list(db);
    if (!state.general_list.empty()) {
      int preferred_id = 0;
      if (!state.owned_file.generals.empty()) {
        preferred_id = state.owned_file.generals.begin()->first;
      } else {
        preferred_id = state.general_list.front().id;
      }

      auto it = std::find_if(state.general_list.begin(), state.general_list.end(),
                             [&](const AnalyzerGeneralListItem& item) {
                               return item.id == preferred_id;
                             });
      if (it == state.general_list.end()) {
        it = state.general_list.begin();
      }
      load_selected_general(db, state, it->id);
    }
  }

  const ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->Pos);
  ImGui::SetNextWindowSize(vp->Size);

  const ImGuiWindowFlags root_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

  ImGui::Begin("##analyzer_workspace", nullptr, root_flags);

  if (ImGui::Button("Save Owned State")) {
    try {
      sync_selected_owned_to_file(state);
      save_state(state);
    } catch (const std::exception& e) {
      state.status_message = std::string("Save failed: ") + e.what();
    }
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(420.0f);
  ImGui::InputText("State File", &state.state_path);
  if (ImGui::Button("Load State File")) {
    try {
      load_state_from_path(state);
      if (state.has_loaded_selected) {
        if (const OwnedGeneralState* existing =
                find_owned_state(state, state.selected_general.id)) {
          state.selected_owned = *existing;
          state.selected_is_owned = true;
        } else {
          state.selected_owned =
              default_owned_state_for_general(state.selected_general);
          state.selected_is_owned = false;
        }
      }
    } catch (const std::exception& e) {
      state.status_message = std::string("Load state failed: ") + e.what();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Create Blank State")) {
    try {
      create_blank_state_at_path(state);
      if (state.has_loaded_selected) {
        state.selected_owned =
            default_owned_state_for_general(state.selected_general);
        state.selected_is_owned = false;
      }
    } catch (const std::exception& e) {
      state.status_message = std::string("Create blank state failed: ") + e.what();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Reload DB List")) {
    try {
      state.general_list = analyzer_load_general_list(db);
      state.status_message = "Reloaded checked general list from DB.";
    } catch (const std::exception& e) {
      state.status_message = std::string("Reload failed: ") + e.what();
    }
  }
  ImGui::SameLine();
  ImGui::TextUnformatted(state.dirty ? "Unsaved changes" : "State synced");

  if (!state.status_message.empty()) {
    ImGui::TextWrapped("%s", state.status_message.c_str());
  }

  ImGui::Separator();

  draw_general_list(db, state);
  ImGui::SameLine();

  ImGui::BeginGroup();
  if (!state.has_loaded_selected) {
    ImGui::TextUnformatted("No checked generals available.");
    ImGui::EndGroup();
    ImGui::End();
    return;
  }

  OwnedGeneralState& owned = state.selected_owned;

  ImGui::Text("%s", state.selected_general.name.c_str());
  ImGui::SameLine();
  if (state.selected_is_owned) {
    draw_status_badge("OWNED", ImVec4(0.35f, 0.85f, 0.35f, 1.0f));
    if (owned.locked) {
      ImGui::SameLine();
      draw_status_badge("LOCKED", ImVec4(0.95f, 0.65f, 0.20f, 1.0f));
    }
  } else {
    draw_status_badge("UNOWNED", ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
  }
  ImGui::Text("Role: %s", state.selected_general.role.c_str());
  ImGui::Text("Covenant: %s", state.selected_general.has_covenant ? "Yes" : "No");
  bool owned_toggle = state.selected_is_owned;
  if (ImGui::Checkbox("Owned", &owned_toggle)) {
    state.selected_is_owned = owned_toggle;
    if (!state.selected_is_owned) {
      owned.locked = false;
    }
    sync_selected_owned_to_file(state);
    state.dirty = true;
  }
  ImGui::SameLine();
  bool locked_toggle = owned.locked;
  ImGui::BeginDisabled(!state.selected_is_owned);
  if (ImGui::Checkbox("Lock", &locked_toggle)) {
    owned.locked = locked_toggle;
    sync_selected_owned_to_file(state);
    state.dirty = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  bool dragon_toggle = owned.has_dragon;
  ImGui::BeginDisabled(!state.selected_is_owned);
  if (ImGui::Checkbox("Has Dragon", &dragon_toggle)) {
    owned.has_dragon = dragon_toggle;
    if (owned.has_dragon) {
      owned.has_spirit_beast = false;
    }
    owned.cached_input_key.clear();
    owned.cached_totals.clear();
    sync_selected_owned_to_file(state);
    state.dirty = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  bool beast_toggle = owned.has_spirit_beast;
  ImGui::BeginDisabled(!state.selected_is_owned);
  if (ImGui::Checkbox("Has Spirit Beast", &beast_toggle)) {
    owned.has_spirit_beast = beast_toggle;
    if (owned.has_spirit_beast) {
      owned.has_dragon = false;
    }
    owned.cached_input_key.clear();
    owned.cached_totals.clear();
    sync_selected_owned_to_file(state);
    state.dirty = true;
  }
  ImGui::EndDisabled();
  if (!state.selected_general.warnings.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s",
                       state.selected_general.warnings.front().c_str());
  }

  bool changed = false;
  const bool edit_disabled = !state.selected_is_owned || owned.locked;
  ImGui::BeginDisabled(edit_disabled);
  changed |= ImGui::InputInt("General Level", &owned.general_level);
  owned.general_level = std::max(0, owned.general_level);
  changed |= ImGui::SliderInt("Ascension Level", &owned.ascension_level, 0, 5);
  for (int i = 0; i < 3; ++i) {
    char label[64];
    std::snprintf(label, sizeof(label), "Specialty %d Level", i + 1);
    changed |= ImGui::SliderInt(label, &owned.specialty_levels[(size_t)i], 0, 5);
  }

  const bool specialty_4_unlocked = owned.specialty_levels[0] >= 5 &&
                                    owned.specialty_levels[1] >= 5 &&
                                    owned.specialty_levels[2] >= 5;
  ImGui::BeginDisabled(!specialty_4_unlocked);
  changed |= ImGui::SliderInt("Specialty 4 Level", &owned.specialty_levels[3], 0,
                              5);
  ImGui::EndDisabled();
  if (!specialty_4_unlocked) {
    owned.specialty_levels[3] = 0;
    ImGui::TextDisabled("Specialty 4 unlocks after specialties 1-3 reach level 5.");
  }

  const int covenant_max =
      state.selected_general.has_covenant ? state.selected_general.covenant_max : 0;
  changed |= ImGui::SliderInt("Covenant Level", &owned.covenant_level, 0,
                              std::max(0, covenant_max));
  ImGui::EndDisabled();

  if (changed) {
    owned.general_name = state.selected_general.name;
    owned.cached_input_key.clear();
    owned.cached_totals.clear();
    sync_selected_owned_to_file(state);
    state.dirty = true;
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Total Stat Values");
  if (!state.selected_is_owned) {
    ImGui::TextDisabled("Check Owned to enable build inputs and totals.");
  } else if (owned.locked) {
    ImGui::TextDisabled("Locked. Uncheck Lock to edit this owned general.");
    const auto totals = compute_total_stats(state.selected_general, owned);
    draw_totals_table(totals);
    draw_combined_summary_rows("combined_summary_main",
                               "Total ",
                               state.selected_general,
                               owned,
                               totals);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextUnformatted("Assistant Stat Values");
    const auto assistant_totals =
        compute_assistant_total_stats(state.selected_general, owned);
    draw_totals_table(assistant_totals);
    draw_combined_summary_rows("combined_summary_assistant",
                               "Assistant Total ",
                               state.selected_general,
                               owned,
                               assistant_totals);
  } else {
    const auto totals = compute_total_stats(state.selected_general, owned);
    draw_totals_table(totals);
    draw_combined_summary_rows("combined_summary_main",
                               "Total ",
                               state.selected_general,
                               owned,
                               totals);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextUnformatted("Assistant Stat Values");
    const auto assistant_totals =
        compute_assistant_total_stats(state.selected_general, owned);
    draw_totals_table(assistant_totals);
    draw_combined_summary_rows("combined_summary_assistant",
                               "Assistant Total ",
                               state.selected_general,
                               owned,
                               assistant_totals);
  }
  ImGui::EndGroup();

  ImGui::End();
  ImGui::PopStyleVar(3);
}

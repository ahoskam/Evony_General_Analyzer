#include "ui.h"
#include "editor_state.h"
#include "model_api.h"
#include "model.h"
#include "db.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cctype>
#include <map>

static void mark_dirty(EditorState& st) { st.dirty = true; }
static bool is_locked(const EditorState& st) { return st.meta.double_checked_in_game != 0; }
static void ui_init(Db& db, EditorState& st);
static void ui_draw(Db& db, EditorState& st);



// Thicker / thinner separators (wide vs slim)
static void ui_separator(float thickness)
{
  ImVec2 p = ImGui::GetCursorScreenPos();
  float w = ImGui::GetContentRegionAvail().x;
  ImGui::GetWindowDrawList()->AddLine(p, ImVec2(p.x + w, p.y), ImGui::GetColorU32(ImGuiCol_Separator), thickness);
  ImGui::Dummy(ImVec2(0.0f, thickness + 3.0f));
}

static int parse_first_int(const std::string& s)
{
  for (size_t i = 0; i < s.size(); ++i) {
    if (std::isdigit((unsigned char)s[i])) {
      int v = 0;
      while (i < s.size() && std::isdigit((unsigned char)s[i])) {
        v = v * 10 + (s[i] - '0');
        ++i;
      }
      return v;
    }
  }
  return 0;
}

static int ascension_index(const Occurrence& o)
{
  // importer uses context_name like "ASCENSION 1"
  if (o.context_type != "Ascension") return 0;
  return parse_first_int(o.context_name);
}

static int covenant_index(const Occurrence& o)
{
  // importer uses context_name like "COVENANT 1"
  if (o.context_type != "Covenant") return 0;
  return parse_first_int(o.context_name);
}

static int specialty_index(const Occurrence& o)
{
  // importer uses context_name like "SPECIALTY 1 L5 (TOTAL)" etc
  if (o.context_type != "Specialty") return 0;
  return parse_first_int(o.context_name);
}

static std::string make_ascension_name(int n) { return "ASCENSION " + std::to_string(n); }
static std::string make_covenant_name(int n)  { return "COVENANT "  + std::to_string(n); }

static std::string make_specialty_name(int n, int level, bool is_total)
{
  // Match your importer style as close as possible:
  // "SPECIALTY 1 L5 (TOTAL)" or "SPECIALTY 1 L3"
  std::string s = "SPECIALTY " + std::to_string(n) + " L" + std::to_string(level);
  if (is_total) s += " (TOTAL)";
  return s;
}

static void add_occurrence(EditorState& st,
                           const std::string& context_type,
                           const std::string& context_name,
                           std::optional<int> level,
                           int is_total)
{
  if (st.stat_keys.empty()) return;

  Occurrence o{};
  o.id = 0; // new
  o.general_id = st.selected_general_id;
  o.context_type = context_type;
  o.context_name = context_name;
  o.level = level;
  o.is_total = is_total;

  // Default to first stat key to avoid "empty" rows
  o.stat_key_id = st.stat_keys.front().id;
  o.stat_key    = st.stat_keys.front().name;
  o.value = 0.0;

  o.origin = "generated";
  o.edited_by_user = 1;

  // provenance for manual rows
  o.file_path = "";
  o.line_number = 0;
  o.raw_line = "";

  st.occ.push_back(std::move(o));
  mark_dirty(st);
}

static bool has_specialty_level_row(const EditorState& st, int spec_n, int stat_key_id, int level)
{
  for (const auto& x : st.occ) {
    if (x.context_type != "Specialty") continue;
    if (specialty_index(x) != spec_n) continue;
    if (x.stat_key_id != stat_key_id) continue;
    if (x.is_total) continue;
    if (!x.level.has_value()) continue;
    if (*x.level == level) return true;
  }
  return false;
}

static void expand_specialty_total(EditorState& st, const Occurrence& total_row)
{
  int spec_n = specialty_index(total_row);
  if (spec_n <= 0) return;

  // Create L1..L5 rows if missing for this specialty + stat_key
  for (int lv = 1; lv <= 5; ++lv) {
    if (has_specialty_level_row(st, spec_n, total_row.stat_key_id, lv)) continue;

    Occurrence o{};
    o.id = 0;
    o.general_id = st.selected_general_id;
    o.context_type = "Specialty";
    o.context_name = make_specialty_name(spec_n, lv, false);
    o.level = lv;
    o.is_total = 0;

    o.stat_key_id = total_row.stat_key_id;
    o.stat_key    = total_row.stat_key;
    o.value = 0.0;

    o.origin = "generated";
    o.edited_by_user = 1;

    if (total_row.id > 0) o.generated_from_total_id = total_row.id;

    o.file_path = "";
    o.line_number = 0;
    o.raw_line = "";

    st.occ.push_back(std::move(o));
  }

  mark_dirty(st);
}





static void clear_current_selection(EditorState& st)
{
  st.selected_general_id = 0;
  st.meta = {};
  st.occ.clear();
  st.pending.clear();
  st.dirty = false;
}

static void reload_current(Db& db, EditorState& st)
{
  if (st.selected_general_id <= 0) return;
  auto all = db_load_all_for_general(db, st.selected_general_id);
  st.meta = std::move(all.meta);
  st.occ  = std::move(all.occ);
  st.pending = std::move(all.pending);
  st.dirty = false;

  // Ensure base skill name exists for BaseSkill context naming
  if (st.meta.base_skill_name.empty()) st.meta.base_skill_name = st.meta.name;
}

static bool save_changes(Db& db, EditorState& st)
{
  st.last_save_error.clear();

  if (st.selected_general_id <= 0) return true;

  try {
    db.begin();

    // Save meta (this function returns false on failure, doesn't throw)
    st.meta.id = st.selected_general_id;
    if (!db_update_general_meta(db, st.selected_general_id, st.meta)) {
      db.rollback();
      st.last_save_error = "db_update_general_meta failed (see model.cpp logging)";
      return false;
    }

    // Delete any rows the user deleted
    for (int id : st.deleted_occurrence_ids) {
      if (id > 0) db_delete_occurrence(db, id);
    }
    st.deleted_occurrence_ids.clear();

    // Upsert occurrences
    for (auto& o : st.occ) {
      if (o.id < 0) continue; // already removed

      o.general_id = st.selected_general_id;
            o.edited_by_user = 1;

      // Don't force origin to an invalid value.
      // Keep whatever it already is (imported/generated).
      // If the row is new or blank, use generated.
      if (o.origin.empty()) o.origin = "generated";


      if (o.id == 0) {
        int new_id = db_insert_occurrence(db, o); // may throw
        if (new_id <= 0) {
          db.rollback();
          st.last_save_error = "db_insert_occurrence returned invalid id";
          return false;
        }
        o.id = new_id;
      } else {
        db_update_occurrence(db, o); // may throw
      }
    }

    db.commit();
    st.dirty = false;
    return true;
  } catch (const std::exception& e) {
    try { db.rollback(); } catch (...) {}
    st.last_save_error = e.what();
    return false;
  }
}


static const char* kRolesAll[] = {
  "All",
  "Unknown",
  "Ground",
  "Mounted",
  "Ranged",
  "Siege",
  "Defense",
  "Mayor",
  "Duty",
  "Mixed"
};

void ui_tick(Db& db, EditorState& st) {
  static bool inited = false;
  if (!inited) { ui_init(db, st); inited = true; }
  ui_draw(db, st);
}

// ---------------------------
// LEFT PANEL (contents only)
// ---------------------------
static void draw_left_contents(Db& db, EditorState& st)
{
  ImGui::TextUnformatted("Filters");
  ImGui::Separator();

  if (ImGui::BeginCombo("Role", st.filter_role.c_str())) {
    for (auto r : kRolesAll) {
      bool sel = (st.filter_role == r);
      if (ImGui::Selectable(r, sel)) st.filter_role = r;
      if (sel) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  ImGui::InputText("Search", &st.filter_name);

  if (ImGui::Button("Refresh")) {
    st.list = db_load_general_list(db, st.filter_role, st.filter_name);
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear")) {
    st.filter_role = "All";
    st.filter_name.clear();
    st.list = db_load_general_list(db, st.filter_role, st.filter_name);
  }

  ImGui::Separator();

  for (const auto& g : st.list) {
    bool selected = (st.selected_general_id == g.id);

    if (g.double_checked_in_game)
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 80, 255));

    if (ImGui::Selectable(g.name.c_str(), selected)) {
      if (st.dirty) {
        st.pending_select_general_id = g.id;
        st.show_dirty_modal = true;
      } else {
        st.selected_general_id = g.id;
        reload_current(db, st);
      }
    }

    if (g.double_checked_in_game)
      ImGui::PopStyleColor();
  }
}

// ---------------------------
// DETAILS PANEL (contents only)
// ---------------------------
static void draw_meta_contents(Db& db, EditorState& st)
{
  (void)db;

  if (st.selected_general_id <= 0) {
    ImGui::TextUnformatted("Select a general on the left.");
    return;
  }

  bool locked = is_locked(st);

  // Save / Reload row
  {
    const bool can_save = (!st.pretty_view);
    if (!can_save) ImGui::BeginDisabled();
    static bool last_save_failed = false;

if (ImGui::Button("Save Changes")) {
  last_save_failed = false;
  if (save_changes(db, st)) {
    // Keep selection and reload from DB so user immediately sees what actually saved.
    reload_current(db, st);
    st.list = db_load_general_list(db, st.filter_role, st.filter_name);
  } else {
    last_save_failed = true;
  }
}

if (last_save_failed) {
  ImGui::Spacing();
  ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "Save failed:");
  if (!st.last_save_error.empty())
    ImGui::TextWrapped("%s", st.last_save_error.c_str());
  else
    ImGui::TextWrapped("(no details)");
}


    if (!can_save) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
      reload_current(db, st);
    }
    ImGui::SameLine();
    ImGui::TextDisabled(st.dirty ? "(unsaved)" : "(saved)");

    // Right-aligned view toggle
    const char* toggle_label = st.pretty_view ? "Edit View" : "Pretty View";
    float btn_w = ImGui::CalcTextSize(toggle_label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    float right_x = ImGui::GetWindowContentRegionMax().x - btn_w;
    ImGui::SameLine();
    ImGui::SetCursorPosX(right_x);
    if (ImGui::Button(toggle_label)) {
      st.pretty_view = !st.pretty_view;
    }
  }

  ImGui::Separator();
  ImGui::Text("Name: %s", st.meta.name.c_str());

  // Pretty view is intentionally read-only (for easy in-game verification)
  if (st.pretty_view) {
    ImGui::Text("Role: %s", st.meta.role.c_str());
    ImGui::Text("Role Confirmed: %s", st.meta.role_confirmed ? "Yes" : "No");
    ImGui::Text("In Tavern: %s", st.meta.in_tavern ? "Yes" : "No");
    ImGui::Text("Double Checked In-Game (LOCK): %s", st.meta.double_checked_in_game ? "Yes" : "No");

    ImGui::Separator();
    ImGui::Text("Leadership: %d  (Green %.3f)", st.meta.leadership, st.meta.leadership_green);
    ImGui::Text("Attack:     %d  (Green %.3f)", st.meta.attack, st.meta.attack_green);
    ImGui::Text("Defense:    %d  (Green %.3f)", st.meta.defense, st.meta.defense_green);
    ImGui::Text("Politics:   %d  (Green %.3f)", st.meta.politics, st.meta.politics_green);

    ImGui::Separator();
    ImGui::Text("Base Skill Name: %s", st.meta.base_skill_name.c_str());

    ImGui::Separator();
    ImGui::TextUnformatted("SOURCE TEXT (VERBATIM):");
    ImGui::BeginDisabled();
    ImGui::InputTextMultiline("##source", &st.meta.source_text_verbatim, ImVec2(-1, 140));
    ImGui::EndDisabled();
    return;
  }

  // lock toggle
  {
    bool lock_bool = st.meta.double_checked_in_game != 0;
    if (ImGui::Checkbox("Double Checked In-Game (LOCK)", &lock_bool)) {
      st.meta.double_checked_in_game = lock_bool ? 1 : 0;
      mark_dirty(st);
    }
  }

  // Role
  if (locked) ImGui::BeginDisabled();
  if (ImGui::BeginCombo("Role", st.meta.role.c_str())) {
    for (auto r : kRolesAll) {
      if (std::string(r) == "All") continue;
      bool sel = (st.meta.role == r);
      if (ImGui::Selectable(r, sel)) { st.meta.role = r; mark_dirty(st); }
      if (sel) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  // int-backed checkboxes
  {
    bool rc = st.meta.role_confirmed != 0;
    if (ImGui::Checkbox("Role Confirmed", &rc)) { st.meta.role_confirmed = rc ? 1 : 0; mark_dirty(st); }
    bool it = st.meta.in_tavern != 0;
    if (ImGui::Checkbox("In Tavern", &it)) { st.meta.in_tavern = it ? 1 : 0; mark_dirty(st); }
  }

  if (locked) ImGui::EndDisabled();
  
  ImGui::Separator();
 if (ImGui::InputInt("Leadership", &st.meta.leadership)) mark_dirty(st);
ImGui::SameLine();
if (ImGui::InputDouble("L Green", &st.meta.leadership_green)) mark_dirty(st);

if (ImGui::InputInt("Attack", &st.meta.attack)) mark_dirty(st);
ImGui::SameLine();
if (ImGui::InputDouble("A Green", &st.meta.attack_green)) mark_dirty(st);

if (ImGui::InputInt("Defense", &st.meta.defense)) mark_dirty(st);
ImGui::SameLine();
if (ImGui::InputDouble("D Green", &st.meta.defense_green)) mark_dirty(st);

if (ImGui::InputInt("Politics", &st.meta.politics)) mark_dirty(st);
ImGui::SameLine();
if (ImGui::InputDouble("P Green", &st.meta.politics_green)) mark_dirty(st);

  // Source text
  ImGui::Separator();
  ImGui::TextUnformatted("SOURCE TEXT (VERBATIM):");
  if (locked) ImGui::BeginDisabled();
  if (ImGui::InputTextMultiline("##source", &st.meta.source_text_verbatim, ImVec2(-1, 140))) mark_dirty(st);
  if (locked) ImGui::EndDisabled();
}

// ---------------------------
// STAT ROW
// ---------------------------
static void draw_stat_row(EditorState& st, Occurrence& o)
{
  bool locked = is_locked(st);
  if (locked) ImGui::BeginDisabled();

  int current = o.stat_key_id;
  const char* preview = o.stat_key.c_str();
  for (auto& k : st.stat_keys) if (k.id == current) { preview = k.name.c_str(); break; }

  ImGui::PushID(o.id ? o.id : (int)(intptr_t)&o);

  if (ImGui::BeginCombo("Stat Key", preview)) {
    for (auto& k : st.stat_keys) {
      bool sel = (k.id == current);
      if (ImGui::Selectable(k.name.c_str(), sel)) {
        o.stat_key_id = k.id;
        o.stat_key = k.name;
        mark_dirty(st);
      }
      if (sel) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  if (ImGui::InputDouble("Value", &o.value)) mark_dirty(st);

  if (ImGui::TreeNode("Provenance")) {
    ImGui::Text("origin: %s", o.origin.c_str());
    if (o.generated_from_total_id.has_value())
      ImGui::Text("generated_from_total_id: %d", *o.generated_from_total_id);
    ImGui::Text("edited_by_user: %d", o.edited_by_user);
    ImGui::Text("file: %s:%d", o.file_path.c_str(), o.line_number);
    ImGui::TextWrapped("raw: %s", o.raw_line.c_str());
    ImGui::TreePop();
  }

  if (ImGui::Button("Delete")) {
    if (o.id > 0) st.deleted_occurrence_ids.push_back(o.id);
    o.id = -1;
    mark_dirty(st);
  }

  ImGui::PopID();

  if (locked) ImGui::EndDisabled();
}

// ---------------------------
// PRETTY (READ-ONLY) STAT ROW
// ---------------------------
static void draw_stat_row_pretty(const Occurrence& o)
{
  // Display value with up to 6 decimals.
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.6f", o.value);
  ImGui::Text("%s: %s", o.stat_key.c_str(), buf);
}

// ---------------------------
// STATS PANEL (contents only)
// ---------------------------
static void draw_occurrences_contents(EditorState& st)
{
  if (st.selected_general_id <= 0) {
    ImGui::TextUnformatted("Select a general.");
    return;
  }

  // remove deleted
  st.occ.erase(std::remove_if(st.occ.begin(), st.occ.end(),
    [](const Occurrence& o){ return o.id == -1; }), st.occ.end());

  const bool pretty = st.pretty_view;
  const bool locked = is_locked(st);

  // ---------------------------
  // 1) Base Skill
  // ---------------------------
  ImGui::TextUnformatted("Base Skill");
  ui_separator(3.0f);

  for (auto& o : st.occ) {
    if (o.context_type == "BaseSkill") {
      if (pretty) draw_stat_row_pretty(o);
      else        draw_stat_row(st, o);
    }
  }

  if (!pretty) {
    if (locked) ImGui::BeginDisabled();
    if (ImGui::Button("Add Base Skill")) {
      add_occurrence(st, "BaseSkill", st.meta.base_skill_name, std::nullopt, 0);
    }
    if (locked) ImGui::EndDisabled();
  }

  ui_separator(3.0f);

  // ---------------------------
  // 2) Ascensions 1..5
  // ---------------------------
  for (int asc = 1; asc <= 5; ++asc) {
    ImGui::Text("Ascension %d", asc);
    ui_separator(1.0f);

    std::string ctx = make_ascension_name(asc);
    for (auto& o : st.occ) {
      if (o.context_type == "Ascension" && o.context_name == ctx) {
        if (pretty) draw_stat_row_pretty(o);
        else        draw_stat_row(st, o);
      }
    }

    if (!pretty) {
      if (locked) ImGui::BeginDisabled();
      std::string btn = "Add Ascension " + std::to_string(asc) + " Skill";
      if (ImGui::Button(btn.c_str())) {
        add_occurrence(st, "Ascension", ctx, std::nullopt, 0);
      }
      if (locked) ImGui::EndDisabled();
    }

    ui_separator(1.0f);
  }

  ui_separator(3.0f);

  // ---------------------------
  // 3) Specialties
  // ---------------------------
  std::map<int, bool> specs;
  for (const auto& o : st.occ) {
    if (o.context_type != "Specialty") continue;
    int n = specialty_index(o);
    if (n > 0) specs[n] = true;
  }

  for (const auto& [spec_n, _] : specs) {
    ImGui::Text("Specialty %d", spec_n);
    ui_separator(1.0f);

    // TOTAL rows first
    for (auto& total : st.occ) {
      if (total.context_type != "Specialty") continue;
      if (specialty_index(total) != spec_n) continue;
      if (!total.is_total) continue;

      ImGui::PushID(total.id ? total.id : (int)(intptr_t)&total);

      if (!pretty) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(160, 40, 40, 255));
        draw_stat_row(st, total);
        ImGui::PopStyleColor();
      } else {
        ImGui::TextUnformatted("TOTAL");
        draw_stat_row_pretty(total);
      }

      bool open = ImGui::TreeNodeEx("Levels", ImGuiTreeNodeFlags_SpanAvailWidth);

      if (!pretty) {
        ImGui::SameLine();
        if (locked) ImGui::BeginDisabled();
        if (ImGui::SmallButton("Expand (create missing)")) {
          expand_specialty_total(st, total);
        }
        if (locked) ImGui::EndDisabled();
      }

      if (open) {
        for (int lv = 1; lv <= 5; ++lv) {
          bool any = false;
          for (const auto& o : st.occ) {
            if (o.context_type != "Specialty") continue;
            if (specialty_index(o) != spec_n) continue;
            if (o.is_total) continue;
            if (!o.level.has_value()) continue;
            if (*o.level != lv) continue;
            if (o.stat_key_id != total.stat_key_id) continue;
            any = true;
            break;
          }
          if (!any) continue;

          ImGui::Text("L%d", lv);
          ui_separator(1.0f);

          for (auto& o : st.occ) {
            if (o.context_type != "Specialty") continue;
            if (specialty_index(o) != spec_n) continue;
            if (o.is_total) continue;
            if (!o.level.has_value()) continue;
            if (*o.level != lv) continue;
            if (o.stat_key_id != total.stat_key_id) continue;

            if (pretty) draw_stat_row_pretty(o);
            else        draw_stat_row(st, o);
          }
        }
        ImGui::TreePop();
      }

      ImGui::PopID();
      ui_separator(1.0f);
    }

    if (!pretty) {
      if (locked) ImGui::BeginDisabled();
      std::string add = "Add Specialty " + std::to_string(spec_n) + " Stat (TOTAL)";
      if (ImGui::Button(add.c_str())) {
        add_occurrence(st, "Specialty", make_specialty_name(spec_n, 5, true), 5, 1);
      }
      if (locked) ImGui::EndDisabled();
    }

    ui_separator(3.0f);
  }

  // ---------------------------
  // 4) Covenants
  // ---------------------------
  std::map<int, bool> covs;
  for (const auto& o : st.occ) {
    if (o.context_type != "Covenant") continue;
    int n = covenant_index(o);
    if (n > 0) covs[n] = true;
  }

  if (!covs.empty()) {
    ImGui::TextUnformatted("Covenants");
    ui_separator(3.0f);
  }

  for (const auto& [c_n, _] : covs) {
    ImGui::Text("Covenant %d", c_n);
    ui_separator(1.0f);

    std::string ctx = make_covenant_name(c_n);
    for (auto& o : st.occ) {
      if (o.context_type == "Covenant" && o.context_name == ctx) {
        if (pretty) draw_stat_row_pretty(o);
        else        draw_stat_row(st, o);
      }
    }

    if (!pretty) {
      if (locked) ImGui::BeginDisabled();
      std::string btn = "Add Covenant " + std::to_string(c_n) + " Stat";
      if (ImGui::Button(btn.c_str())) {
        add_occurrence(st, "Covenant", ctx, std::nullopt, 0);
      }
      if (locked) ImGui::EndDisabled();
    }

    ui_separator(3.0f);
  }
}



// ---------------------------
// PENDING PANEL (contents only)
// ---------------------------
static void draw_pending_contents(Db& /*db*/, EditorState& st)
{
  if (st.selected_general_id <= 0) {
    ImGui::TextUnformatted("Select a general.");
    return;
  }

  if (st.pending.empty()) {
    ImGui::TextUnformatted("No pending examples for this general.");
    return;
  }

  for (auto& p : st.pending) {
    ImGui::Separator();
    ImGui::Text("raw_key: %s", p.raw_key.c_str());
    ImGui::Text("value: %.6f", p.value);
    ImGui::Text("context: %s / %s", p.context_type.c_str(), p.context_name.c_str());
    if (p.level.has_value()) ImGui::Text("level: %d", *p.level);
    ImGui::Text("file: %s:%d", p.file_path.c_str(), p.line_number);
    ImGui::TextWrapped("raw: %s", p.raw_line.c_str());
  }
}

void ui_init(Db& db, EditorState& st)
{
  st.filter_role = "All";
  st.list = db_load_general_list(db, st.filter_role, st.filter_name);
  st.stat_keys = db_load_stat_keys(db);
}

void ui_draw(Db& db, EditorState& st)
{
  // Dirty modal (unchanged)
  if (st.show_dirty_modal) {
    ImGui::OpenPopup("Unsaved changes");
    st.show_dirty_modal = false;
  }

  if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("You have unsaved changes. Save now?");

    if (ImGui::Button("Save")) {
      if (save_changes(db, st)) {
        if (st.pending_select_general_id > 0) {
          st.selected_general_id = st.pending_select_general_id;
          st.pending_select_general_id = 0;
          reload_current(db, st);
        } else {
          clear_current_selection(st);
        }
      }
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
      if (st.pending_select_general_id > 0) {
        st.selected_general_id = st.pending_select_general_id;
        st.pending_select_general_id = 0;
        reload_current(db, st);
      } else if (st.selected_general_id > 0) {
        reload_current(db, st);
      } else {
        clear_current_selection(st);
      }
      st.dirty = false;
      st.deleted_occurrence_ids.clear();
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      st.pending_select_general_id = 0;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  // ============================================================
  // SINGLE WORKSPACE WINDOW (prevents floating windows)
  // ============================================================
  const ImGuiViewport* vp = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(vp->Pos);
  ImGui::SetNextWindowSize(vp->Size);
  //ImGui::SetNextWindowViewport(vp->ID);

  const ImGuiWindowFlags root_flags =
      ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_MenuBar;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

  if (ImGui::Begin("##workspace", nullptr, root_flags)) {
    if (ImGui::BeginMenuBar()) {
      ImGui::TextUnformatted("Evony General Analyzer");
      ImGui::EndMenuBar();
    }

    const float left_w  = 280.0f;
    const float avail_h = ImGui::GetContentRegionAvail().y;

    // LEFT
    ImGui::BeginChild("##left", ImVec2(left_w, avail_h), true);
    ImGui::TextUnformatted("Generals");
    ImGui::Separator();
    draw_left_contents(db, st);
    ImGui::EndChild();

    ImGui::SameLine();

    // RIGHT
    ImGui::BeginChild("##right", ImVec2(0, avail_h), false);

    const float details_h = 300.0f;
    ImGui::BeginChild("##details", ImVec2(0, details_h), true);
    ImGui::TextUnformatted("Details");
    ImGui::Separator();
    draw_meta_contents(db, st);
    ImGui::EndChild();

    ImGui::Spacing();

    ImGui::BeginChild("##bottom", ImVec2(0, 0), false);

    if (ImGui::BeginTable("##bottom_table", 2,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
      ImGui::TableSetupColumn("Stats",   ImGuiTableColumnFlags_WidthStretch, 0.75f);
      ImGui::TableSetupColumn("Pending", ImGuiTableColumnFlags_WidthStretch, 0.25f);
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::BeginChild("##stats", ImVec2(0, 0), true);
      ImGui::TextUnformatted("Stats");
      ImGui::Separator();
      draw_occurrences_contents(st);
      ImGui::EndChild();

      ImGui::TableSetColumnIndex(1);
      ImGui::BeginChild("##pending", ImVec2(0, 0), true);
      ImGui::TextUnformatted("Pending (Unmapped) Stat Keys");
      ImGui::Separator();
      draw_pending_contents(db, st);
      ImGui::EndChild();

      ImGui::EndTable();
    }

    ImGui::EndChild(); // bottom
    ImGui::EndChild(); // right
  }

  ImGui::End();
  ImGui::PopStyleVar(3);
}

#include "ui.h"
#include "db.h"
#include "editor_state.h"
#include "model.h"
#include "model_api.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

static void mark_dirty(EditorState &st) { st.dirty = true; }
static bool is_locked(const EditorState &st) {
  return st.meta.double_checked_in_game != 0;
}
static void ui_init(Db &db, EditorState &st);
static void ui_draw(Db &db, EditorState &st);

// Thicker / thinner separators (wide vs slim)
static void ui_separator(float thickness) {
  ImVec2 p = ImGui::GetCursorScreenPos();
  float w = ImGui::GetContentRegionAvail().x;
  ImGui::GetWindowDrawList()->AddLine(p, ImVec2(p.x + w, p.y),
                                      ImGui::GetColorU32(ImGuiCol_Separator),
                                      thickness);
  ImGui::Dummy(ImVec2(0.0f, thickness + 3.0f));
}

static void ui_section_bar(const char *label, ImU32 bg_col) {
  ImDrawList *dl = ImGui::GetWindowDrawList();
  ImVec2 p = ImGui::GetCursorScreenPos();
  float w = ImGui::GetContentRegionAvail().x;
  float h =
      ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;

  dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg_col, 4.0f);

  ImGui::SetCursorScreenPos(ImVec2(p.x + ImGui::GetStyle().FramePadding.x,
                                   p.y + ImGui::GetStyle().FramePadding.y));
  ImGui::TextUnformatted(label);

  ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 4.0f));
}

static int parse_first_int(const std::string &s) {
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

static int pressed_alpha_key_upper() {
  for (int i = 0; i < 26; ++i) {
    ImGuiKey key = static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_A) + i);
    if (ImGui::IsKeyPressed(key, false))
      return 'A' + i;
  }
  return 0;
}

static bool starts_with_alpha_ci(const std::string &text, int upper_ch) {
  if (upper_ch == 0 || text.empty())
    return false;
  return std::toupper(static_cast<unsigned char>(text.front())) == upper_ch;
}

static std::string stat_key_display_name(const StatKey &k) {
  return k.is_active ? k.name : (k.name + " *");
}

/* static int ascension_index(const Occurrence& o)
{
  // importer uses context_name like "ASCENSION 1"
  if (o.context_type != "Ascension") return 0;
  return parse_first_int(o.context_name);
} */
static int ascension_index(const Occurrence &o) {
  // Support both "ASCENSION 1" and "Ascension 1"
  if (o.context_type != "Ascension")
    return 0;
  return parse_first_int(o.context_name);
}

static int covenant_index(const Occurrence &o) {
  // importer uses context_name like "COVENANT 1"
  if (o.context_type != "Covenant")
    return 0;
  return parse_first_int(o.context_name);
}

static int specialty_index(const Occurrence &o) {
  // importer uses context_name like "SPECIALTY 1 L5 (TOTAL)" etc
  if (o.context_type != "Specialty")
    return 0;
  return parse_first_int(o.context_name);
}

static std::string make_ascension_name(int n) {
  return "ASCENSION " + std::to_string(n);
}
static std::string make_covenant_name(int n) {
  return "COVENANT " + std::to_string(n);
}

static std::string make_specialty_name(int n, int level, bool is_total) {
  // Match your importer style as close as possible:
  // "SPECIALTY 1 L5 (TOTAL)" or "SPECIALTY 1 L3"
  std::string s =
      "SPECIALTY " + std::to_string(n) + " L" + std::to_string(level);
  if (is_total)
    s += " (TOTAL)";
  return s;
}

static void add_occurrence(EditorState &st, const std::string &context_type,
                           const std::string &context_name,
                           std::optional<int> level, int is_total) {
  if (st.stat_keys.empty())
    return;

  Occurrence o{};
  o.id = 0; // new
  o.general_id = st.selected_general_id;
  o.context_type = context_type;
  o.context_name = context_name;
  o.level = level;
  o.is_total = is_total;

  // Default to first stat key to avoid "empty" rows
  o.stat_key_id = st.stat_keys.front().id;
  o.stat_key = st.stat_keys.front().name;
  o.value = 0.0;

  o.origin = "gui";
  o.edited_by_user = 1;
  o.stat_checked_in_game = 0;

  // provenance for manual rows
  o.file_path = "";
  o.line_number = 0;
  o.raw_line = "";

  st.occ.push_back(std::move(o));
  mark_dirty(st);
}

static bool has_specialty_level_row(const EditorState &st, int spec_n,
                                    int stat_key_id, int level) {
  for (const auto &x : st.occ) {
    if (x.context_type != "Specialty")
      continue;
    if (specialty_index(x) != spec_n)
      continue;
    if (x.stat_key_id != stat_key_id)
      continue;
    if (x.is_total)
      continue;
    if (!x.level.has_value())
      continue;
    if (*x.level == level)
      return true;
  }
  return false;
}

static std::optional<std::vector<int>>
split_total_abs_to_increments(double abs_total) {
  // Match by integer totals. Patterns you’ve verified:
  // 10 => 1,1,2,2,4
  // 15 => 1,2,3,3,6
  // 6  => 1,1,1,1,2
  // 50 => 5,5,10,10,20
  // 16 => 2,2,3,3,6
  // 20 => 2,2,4,4,8
  // 25 => 2,3,5,5,10
  // 26 => 3,3,5,5,10
  // 30 => 3,3,6,6,12
  // 35 => 3,4,6,7,15
  // 36 => 4,4,7,7,14
  // 40 => 4,4,8,8,16
  // 45 => 4,5,9,9,18
  // 46 => 5,5,9,9,18
  static const std::unordered_map<int, std::vector<int>> patterns = {
      {10, {1, 1, 2, 2, 4}},  {15, {1, 2, 3, 3, 6}},  {6, {1, 1, 1, 1, 2}},
      {50, {5, 5, 10, 10, 20}},
      {16, {2, 2, 3, 3, 6}},  {20, {2, 2, 4, 4, 8}},  {25, {2, 3, 5, 5, 10}},
      {26, {3, 3, 5, 5, 10}}, {30, {3, 3, 6, 6, 12}}, {35, {3, 4, 6, 7, 15}},
      {36, {4, 4, 7, 7, 14}}, {40, {4, 4, 8, 8, 16}}, {45, {4, 5, 9, 9, 18}},
      {46, {5, 5, 9, 9, 18}},
  };

  int t = (int)std::lround(abs_total);
  auto it = patterns.find(t);
  if (it == patterns.end())
    return std::nullopt;
  return it->second;
}

static void expand_specialty_total(EditorState &st,
                                   const Occurrence &total_row) {
  int spec_n = specialty_index(total_row);
  if (spec_n <= 0)
    return;

  // Create L1..L5 rows if missing for this specialty + stat_key
  for (int lv = 1; lv <= 5; ++lv) {
    if (has_specialty_level_row(st, spec_n, total_row.stat_key_id, lv))
      continue;

    Occurrence o{};
    o.id = 0;
    o.general_id = st.selected_general_id;
    o.context_type = "Specialty";
    o.context_name = make_specialty_name(spec_n, lv, false);
    o.level = lv;
    o.is_total = 0;

    o.stat_key_id = total_row.stat_key_id;
    o.stat_key = total_row.stat_key;
    o.value = 0.0;

    o.origin = "generated";
    o.edited_by_user = 1;
    o.stat_checked_in_game = 0;

    if (total_row.id > 0)
      o.generated_from_total_id = total_row.id;

    o.file_path = "";
    o.line_number = 0;
    o.raw_line = "";

    st.occ.push_back(std::move(o));
  }

  mark_dirty(st);
}

static void clear_current_selection(EditorState &st) {
  st.selected_general_id = 0;
  st.meta = {};
  st.occ.clear();
  st.pending.clear();
  st.pending_chosen_stat_key_id.clear();
  st.show_unmapped_delete_modal = false;
  st.pending_unmapped_delete_pending_id = 0;
  st.pending_unmapped_delete_context_type.clear();
  st.pending_unmapped_delete_context_index = 0;
  st.pending_unmapped_delete_raw_key.clear();
  st.dirty = false;
}

static void reload_current(Db &db, EditorState &st) {
  if (st.selected_general_id <= 0)
    return;
  auto all = db_load_all_for_general(db, st.selected_general_id);
  st.meta = std::move(all.meta);
  st.occ = std::move(all.occ);
  st.pending = std::move(all.pending);
  st.pending_chosen_stat_key_id.assign(st.pending.size(),
                                       st.stat_keys.empty() ? 0
                                                            : st.stat_keys.front().id);
  st.dirty = false;

  // Ensure base skill name exists for BaseSkill context naming
  if (st.meta.base_skill_name.empty())
    st.meta.base_skill_name = st.meta.name;
}

static bool save_changes(Db &db, EditorState &st) {
  st.last_save_error.clear();

  if (st.selected_general_id <= 0)
    return true;

  try {
    db.begin();

    // Save meta (this function returns false on failure, doesn't throw)
    st.meta.id = st.selected_general_id;
    if (!db_update_general_meta(db, st.selected_general_id, st.meta)) {
      db.rollback();
      st.last_save_error =
          "db_update_general_meta failed (see model.cpp logging)";
      return false;
    }

    // Delete any rows the user deleted
    for (int id : st.deleted_occurrence_ids) {
      if (id > 0)
        db_delete_occurrence(db, id);
    }
    st.deleted_occurrence_ids.clear();

    // Upsert occurrences
    for (auto &o : st.occ) {
      if (o.id < 0)
        continue; // already removed

      o.general_id = st.selected_general_id;
      o.edited_by_user = 1;

      // Keep existing provenance where possible.
      // If row is new or blank, treat it as user-entered.
      if (o.origin.empty())
        o.origin = "gui";

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

    // ------------------------------------------------------------
    // Auto-expand Specialty L5 (TOTAL) rows into L1..L5 increments
    // when no level rows exist yet, using known patterns.
    // This materializes the rows into DB on Save (no restart needed).
    // ------------------------------------------------------------
    struct SpecialtyLevelShape {
      bool has_level_1to4 = false;
      std::vector<int> level5_indices;
    };
    auto inspect_level_shape_for = [&](int spec_n,
                                       int stat_key_id) -> SpecialtyLevelShape {
      SpecialtyLevelShape out{};
      for (int i = 0; i < (int)st.occ.size(); ++i) {
        const auto &x = st.occ[(size_t)i];
        if (x.context_type != "Specialty")
          continue;
        if (specialty_index(x) != spec_n)
          continue;
        if (x.stat_key_id != stat_key_id)
          continue;
        if (x.is_total)
          continue;
        if (!x.level.has_value())
          continue;
        const int lv = *x.level;
        if (lv >= 1 && lv <= 4) {
          out.has_level_1to4 = true;
          continue;
        }
        if (lv == 5)
          out.level5_indices.push_back(i);
      }
      return out;
    };

    std::vector<Occurrence> to_add;
    to_add.reserve(64);

    for (const auto &total : st.occ) {
      if (total.context_type != "Specialty")
        continue;
      if (!total.is_total)
        continue;
      if (!total.level.has_value() || *total.level != 5)
        continue;

      int spec_n = specialty_index(total);
      if (spec_n <= 0)
        continue;

      double v = total.value;
      double abs_total = std::fabs(v);
      auto inc_opt = split_total_abs_to_increments(abs_total);
      if (!inc_opt.has_value())
        continue;

      const auto &inc = *inc_opt;
      if (inc.size() != 5)
        continue;

      double sign = (v < 0.0) ? -1.0 : 1.0;

      const auto level_shape = inspect_level_shape_for(spec_n, total.stat_key_id);
      if (level_shape.has_level_1to4)
        continue;

      int legacy_singleton_l5_idx = -1;
      if (!level_shape.level5_indices.empty()) {
        if (level_shape.level5_indices.size() == 1) {
          const int idx = level_shape.level5_indices.front();
          const auto &existing_l5 = st.occ[(size_t)idx];
          if (std::fabs(std::fabs(existing_l5.value) - abs_total) < 1e-9) {
            // Legacy "L5-only Max Level Attributes" row stored as a level row.
            // Reuse it as the L5 increment instead of creating a duplicate row.
            legacy_singleton_l5_idx = idx;
          } else {
            continue;
          }
        } else {
          continue;
        }
      }

      for (int lv = 1; lv <= 4; ++lv) {
        Occurrence o{};
        o.id = 0;
        o.general_id = st.selected_general_id;
        o.context_type = "Specialty";
        o.context_name = make_specialty_name(spec_n, lv, false);
        o.level = lv;
        o.is_total = 0;

        o.stat_key_id = total.stat_key_id;
        o.stat_key = total.stat_key;
        o.value = sign * (double)inc[(size_t)lv - 1];

        o.origin = "generated";
        o.edited_by_user = 1;
        o.stat_checked_in_game = 0;
        if (total.id > 0)
          o.generated_from_total_id = total.id;

        o.file_path = "";
        o.line_number = 0;
        o.raw_line = "";

        to_add.push_back(std::move(o));
      }

      if (legacy_singleton_l5_idx >= 0) {
        auto &existing_l5 = st.occ[(size_t)legacy_singleton_l5_idx];
        existing_l5.value = sign * (double)inc[4];
        existing_l5.context_name = make_specialty_name(spec_n, 5, false);
        existing_l5.level = 5;
        existing_l5.is_total = 0;
        existing_l5.origin = "generated";
        existing_l5.raw_line = "NORMALIZED_SINGLETON_L5_TO_INCREMENT";

        if (existing_l5.id > 0) {
          db_update_occurrence(db, existing_l5);
        } else {
          int new_id = db_insert_occurrence(db, existing_l5);
          if (new_id <= 0) {
            db.rollback();
            st.last_save_error =
                "Auto-expand singleton L5 normalize insert failed";
            return false;
          }
          existing_l5.id = new_id;
        }
      } else {
        Occurrence o{};
        o.id = 0;
        o.general_id = st.selected_general_id;
        o.context_type = "Specialty";
        o.context_name = make_specialty_name(spec_n, 5, false);
        o.level = 5;
        o.is_total = 0;

        o.stat_key_id = total.stat_key_id;
        o.stat_key = total.stat_key;
        o.value = sign * (double)inc[4];

        o.origin = "generated";
        o.edited_by_user = 1;
        o.stat_checked_in_game = 0;
        if (total.id > 0)
          o.generated_from_total_id = total.id;

        o.file_path = "";
        o.line_number = 0;
        o.raw_line = "";

        to_add.push_back(std::move(o));
      }
    }

    for (auto &o : to_add) {
      int new_id = db_insert_occurrence(db, o);
      if (new_id <= 0) {
        db.rollback();
        st.last_save_error = "Auto-expand insert failed (invalid id)";
        return false;
      }
      o.id = new_id;
      st.occ.push_back(o); // keep UI in sync inside this save call
    }

    // ------------------------------------------------------------
    // Auto-create missing Specialty L5 (TOTAL) rows from level rows.
    // This handles imports that have only L1..L5 rows and no explicit TOTAL.
    // ------------------------------------------------------------
    struct LevelSumKey {
      int spec_n = 0;
      int stat_key_id = 0;
      bool operator==(const LevelSumKey &r) const {
        return spec_n == r.spec_n && stat_key_id == r.stat_key_id;
      }
    };
    struct LevelSumKeyHash {
      std::size_t operator()(const LevelSumKey &k) const {
        return (std::size_t)k.spec_n * 1315423911u ^ (std::size_t)k.stat_key_id;
      }
    };
    struct LevelSumVal {
      double sum = 0.0;
      std::string stat_key;
      int level_rows = 0;
      bool has_level_1to4 = false;
    };

    auto has_total_for = [&](int spec_n, int stat_key_id) -> bool {
      for (const auto &x : st.occ) {
        if (x.context_type != "Specialty")
          continue;
        if (specialty_index(x) != spec_n)
          continue;
        if (!x.is_total)
          continue;
        if (x.stat_key_id == stat_key_id)
          return true;
      }
      return false;
    };

    std::unordered_map<LevelSumKey, LevelSumVal, LevelSumKeyHash> level_sums;
    for (const auto &x : st.occ) {
      if (x.context_type != "Specialty")
        continue;
      if (x.is_total)
        continue;
      if (!x.level.has_value())
        continue;
      const int lv = *x.level;
      if (lv < 1 || lv > 5)
        continue;
      const int spec_n = specialty_index(x);
      if (spec_n <= 0)
        continue;

      LevelSumKey k{spec_n, x.stat_key_id};
      auto &v = level_sums[k];
      v.sum += x.value;
      v.stat_key = x.stat_key;
      v.level_rows += 1;
      if (lv >= 1 && lv <= 4)
        v.has_level_1to4 = true;
    }

    std::vector<Occurrence> totals_to_add;
    totals_to_add.reserve(level_sums.size());
    for (const auto &[k, v] : level_sums) {
      if (v.level_rows <= 0)
        continue;
      if (has_total_for(k.spec_n, k.stat_key_id))
        continue;
      if (!v.has_level_1to4 && v.level_rows == 1)
        continue; // ambiguous singleton L5 row; do not auto-create TOTAL

      Occurrence o{};
      o.id = 0;
      o.general_id = st.selected_general_id;
      o.context_type = "Specialty";
      o.context_name = make_specialty_name(k.spec_n, 5, true);
      o.level = 5;
      o.is_total = 1;

      o.stat_key_id = k.stat_key_id;
      o.stat_key = v.stat_key;
      o.value = v.sum;

      o.origin = "generated";
      o.edited_by_user = 1;
      o.stat_checked_in_game = 0;
      o.file_path = "";
      o.line_number = 0;
      o.raw_line = "";

      totals_to_add.push_back(std::move(o));
    }

    for (auto &o : totals_to_add) {
      int new_id = db_insert_occurrence(db, o);
      if (new_id <= 0) {
        db.rollback();
        st.last_save_error = "Auto-total insert failed (invalid id)";
        return false;
      }
      o.id = new_id;
      st.occ.push_back(o); // keep UI in sync inside this save call
    }

    db.commit();
    st.dirty = false;
    return true;
  } catch (const std::exception &e) {
    try {
      db.rollback();
    } catch (...) {
    }
    st.last_save_error = e.what();
    return false;
  }
}

static const char *kRolesAll[] = {"All",    "Ground", "Mounted", "Ranged",
                                  "Siege",  "Defense", "Mixed",   "Admin",
                                  "Duty",   "Mayor",   "Unknown"};

void ui_tick(Db &db, EditorState &st) {
  static bool inited = false;
  if (!inited) {
    ui_init(db, st);
    inited = true;
  }
  ui_draw(db, st);
}

// ---------------------------
// LEFT PANEL (contents only)
// ---------------------------
static void draw_left_contents(Db &db, EditorState &st) {
  // Local flag bits (must match model.cpp)
  enum GeneralStatusFlags : int {
    GS_MISSING_SOURCE_TEXT = 1 << 0,
    GS_MISSING_BASE_SKILL = 1 << 1,
    GS_MISSING_ASCENSIONS = 1 << 2,
    GS_MISSING_SPECIALTIES = 1 << 3,
    GS_MISSING_COVENANT_6 = 1 << 4,
  };

  ImGui::TextUnformatted("Filters");
  ImGui::Separator();

  if (ImGui::BeginCombo("Role", st.filter_role.c_str())) {
    for (auto r : kRolesAll) {
      bool sel = (st.filter_role == r);
      if (ImGui::Selectable(r, sel))
        st.filter_role = r;
      if (sel)
        ImGui::SetItemDefaultFocus();
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

  // Legend (optional but helpful)
  ImGui::TextDisabled("LOCK = yellow");
  ImGui::TextDisabled("NO SRC = red");
  ImGui::TextDisabled("NO BASE = orange");
  ImGui::TextDisabled("ASC<5 = purple");
  ImGui::TextDisabled("SPEC<4 = cyan");
  ImGui::TextDisabled("COV<6 = green");

  ImGui::Separator();

  for (const auto &g : st.list) {
    bool selected = (st.selected_general_id == g.id);

    // Build label with tags so you can see WHY it’s colored
    std::string label = g.name;

    const int flags = g.status_flags;

    if (flags & GS_MISSING_SOURCE_TEXT)
      label += " [NO SRC]";
    if (flags & GS_MISSING_BASE_SKILL)
      label += " [NO BASE]";
    if (flags & GS_MISSING_ASCENSIONS)
      label += " [ASC<5]";
    if (flags & GS_MISSING_SPECIALTIES)
      label += " [SPEC<4]";
    if (flags & GS_MISSING_COVENANT_6)
      label += " [COV<6]";

    // Color priority:
    // 1) locked yellow overrides all
    // 2) missing source
    // 3) missing base
    // 4) missing ascensions
    // 5) missing specialties
    // 6) has covenants but missing 6th
    bool pushed = false;

    if (g.double_checked_in_game) {
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 80, 255));
      pushed = true;
    } else if (flags & GS_MISSING_SOURCE_TEXT) {
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 90, 90, 255));
      pushed = true;
    } else if (flags & GS_MISSING_BASE_SKILL) {
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 170, 80, 255));
      pushed = true;
    } else if (flags & GS_MISSING_ASCENSIONS) {
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 140, 255, 255));
      pushed = true;
    } else if (flags & GS_MISSING_SPECIALTIES) {
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 220, 255, 255));
      pushed = true;
    } else if (flags & GS_MISSING_COVENANT_6) {
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 255, 140, 255));
      pushed = true;
    }

    if (ImGui::Selectable(label.c_str(), selected)) {
      if (st.dirty) {
        st.pending_select_general_id = g.id;
        st.show_dirty_modal = true;
      } else {
        st.selected_general_id = g.id;
        reload_current(db, st);
      }
    }

    if (pushed)
      ImGui::PopStyleColor();
  }
}

// ---------------------------
// DETAILS PANEL (contents only)
// ---------------------------
static void draw_meta_contents(Db &db, EditorState &st) {
  (void)db;

  if (st.selected_general_id <= 0) {
    ImGui::TextUnformatted("Select a general on the left.");
    return;
  }

  bool locked = is_locked(st);

  // Save / Reload row
  {
    const bool can_save = (!st.pretty_view);
    if (!can_save)
      ImGui::BeginDisabled();
    static bool last_save_failed = false;

    if (ImGui::Button("Save Changes")) {
      last_save_failed = false;
      if (save_changes(db, st)) {
        // Keep selection and reload from DB so user immediately sees what
        // actually saved.
        reload_current(db, st);
        st.list = db_load_general_list(db, st.filter_role, st.filter_name);
      } else {
        last_save_failed = true;
      }
    }

    if (last_save_failed) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Save failed:");
      if (!st.last_save_error.empty())
        ImGui::TextWrapped("%s", st.last_save_error.c_str());
      else
        ImGui::TextWrapped("(no details)");
    }

    if (!can_save)
      ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
      reload_current(db, st);
    }
    ImGui::SameLine();
    ImGui::TextDisabled(st.dirty ? "(unsaved)" : "(saved)");

    // Right-aligned view toggle
    const char *toggle_label = st.pretty_view ? "Edit View" : "Pretty View";
    float btn_w = ImGui::CalcTextSize(toggle_label).x +
                  ImGui::GetStyle().FramePadding.x * 2.0f;
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
    ImGui::Text("Double Checked In-Game (LOCK): %s",
                st.meta.double_checked_in_game ? "Yes" : "No");

    ImGui::Separator();
    ImGui::Text("Leadership: %d  (Green %.2f)", st.meta.leadership,
                st.meta.leadership_green);
    ImGui::Text("Attack:     %d  (Green %.2f)", st.meta.attack,
                st.meta.attack_green);
    ImGui::Text("Defense:    %d  (Green %.2f)", st.meta.defense,
                st.meta.defense_green);
    ImGui::Text("Politics:   %d  (Green %.2f)", st.meta.politics,
                st.meta.politics_green);

    ImGui::Separator();
    ImGui::Text("Base Skill Name: %s", st.meta.base_skill_name.c_str());

    ImGui::Separator();
    ImGui::TextUnformatted("SOURCE TEXT (VERBATIM):");
    // Soft wrap at ~120 characters for display
    auto wrap_text_120 = [](const std::string &in) {
      const int max_width = 120;

      std::string out;
      out.reserve(in.size() + in.size() / 80);

      std::string line;
      line.reserve(256);

      auto flush_wrapped_line = [&](const std::string &src_line) {
        size_t i = 0;
        const size_t n = src_line.size();

        while (i < n) {
          // Skip leading spaces/tabs (but don't destroy indentation completely)
          // Keep up to a small indentation prefix if present.
          size_t indent_start = i;
          while (indent_start < n && (src_line[indent_start] == ' ' ||
                                      src_line[indent_start] == '\t'))
            indent_start++;

          // Measure indentation (cap it so it doesn't eat the whole line)
          std::string indent =
              src_line.substr(i, std::min(indent_start - i, (size_t)12));
          i = indent_start;

          int line_len = 0;
          if (!indent.empty()) {
            out += indent;
            line_len += (int)indent.size();
          }

          // If we're at end after whitespace, emit newline and continue
          if (i >= n) {
            out += '\n';
            break;
          }

          // Wrap the remainder of this "paragraph line" until newline
          while (i < n) {
            // Determine the next break candidate window
            size_t remaining = n - i;
            int room = max_width - line_len;

            if (room <= 1) {
              out += '\n';
              // Reapply indent on wrapped continuation lines
              if (!indent.empty())
                out += indent;
              line_len = (int)indent.size();
              room = max_width - line_len;
            }

            // If the remaining text fits, write it and finish the line
            if ((int)remaining <= room) {
              out.append(src_line, i, remaining);
              out += '\n';
              i = n;
              break;
            }

            // Find last whitespace within the room (prefer breaking at
            // whitespace)
            size_t break_pos = std::string::npos;
            size_t search_end = i + (size_t)room;
            if (search_end > n)
              search_end = n;

            for (size_t j = search_end; j > i; --j) {
              char c = src_line[j - 1];
              if (c == ' ' || c == '\t') {
                break_pos = j - 1;
                break;
              }
            }

            if (break_pos == std::string::npos) {
              // No whitespace found: hard-break (very long word)
              out.append(src_line, i, (size_t)room);
              out += '\n';
              if (!indent.empty())
                out += indent;
              i += (size_t)room;
              line_len = (int)indent.size();
              // Skip any whitespace after hard break
              while (i < n && (src_line[i] == ' ' || src_line[i] == '\t'))
                i++;
            } else {
              // Break at whitespace
              size_t chunk_len = break_pos - i;
              out.append(src_line, i, chunk_len);
              out += '\n';
              if (!indent.empty())
                out += indent;
              i = break_pos + 1; // skip the space/tab we broke on
              while (i < n && (src_line[i] == ' ' || src_line[i] == '\t'))
                i++;
              line_len = (int)indent.size();
            }
          }
        }
      };

      // Process line-by-line preserving existing newlines exactly
      size_t start = 0;
      while (start <= in.size()) {
        size_t end = in.find('\n', start);
        if (end == std::string::npos)
          end = in.size();

        std::string src_line = in.substr(start, end - start);
        flush_wrapped_line(src_line);

        if (end == in.size())
          break;
        start = end + 1;
      }

      return out;
    };

    std::string wrapped = wrap_text_120(st.meta.source_text_verbatim);

    ImGui::BeginDisabled();
    ImGui::InputTextMultiline("##source", &wrapped, ImVec2(-1, 160));
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
  if (locked)
    ImGui::BeginDisabled();
  if (ImGui::BeginCombo("Role", st.meta.role.c_str())) {
    for (auto r : kRolesAll) {
      if (std::string(r) == "All")
        continue;
      bool sel = (st.meta.role == r);
      if (ImGui::Selectable(r, sel)) {
        st.meta.role = r;
        mark_dirty(st);
      }
      if (sel)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  // int-backed checkboxes
  {
    bool rc = st.meta.role_confirmed != 0;
    if (ImGui::Checkbox("Role Confirmed", &rc)) {
      st.meta.role_confirmed = rc ? 1 : 0;
      mark_dirty(st);
    }
    bool it = st.meta.in_tavern != 0;
    if (ImGui::Checkbox("In Tavern", &it)) {
      st.meta.in_tavern = it ? 1 : 0;
      mark_dirty(st);
    }
  }

  if (locked)
    ImGui::EndDisabled();

  ImGui::Separator();
  if (ImGui::InputInt("Leadership", &st.meta.leadership))
    mark_dirty(st);
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(40, 120, 40, 110));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(40, 120, 40, 140));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(40, 120, 40, 170));
  if (ImGui::InputDouble("L Green", &st.meta.leadership_green, 0.01, 0.1,
                         "%.2f"))
    mark_dirty(st);
  ImGui::PopStyleColor(3);

  if (ImGui::InputInt("Attack", &st.meta.attack))
    mark_dirty(st);
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(40, 120, 40, 110));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(40, 120, 40, 140));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(40, 120, 40, 170));
  if (ImGui::InputDouble("A Green", &st.meta.attack_green, 0.01, 0.1, "%.2f"))
    mark_dirty(st);
  ImGui::PopStyleColor(3);

  if (ImGui::InputInt("Defense", &st.meta.defense))
    mark_dirty(st);
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(40, 120, 40, 110));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(40, 120, 40, 140));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(40, 120, 40, 170));
  if (ImGui::InputDouble("D Green", &st.meta.defense_green, 0.01, 0.1, "%.2f"))
    mark_dirty(st);
  ImGui::PopStyleColor(3);

  if (ImGui::InputInt("Politics", &st.meta.politics))
    mark_dirty(st);
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(40, 120, 40, 110));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(40, 120, 40, 140));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(40, 120, 40, 170));
  if (ImGui::InputDouble("P Green", &st.meta.politics_green, 0.01, 0.1, "%.2f"))
    mark_dirty(st);
  ImGui::PopStyleColor(3);

  // Source text
  ImGui::Separator();
  ImGui::TextUnformatted("SOURCE TEXT (VERBATIM):");
  if (locked)
    ImGui::BeginDisabled();
  if (ImGui::InputTextMultiline("##source", &st.meta.source_text_verbatim,
                                ImVec2(-1, 140)))
    mark_dirty(st);
  if (locked)
    ImGui::EndDisabled();
}

// ---------------------------
// STAT ROW
// ---------------------------
static void draw_stat_row(EditorState &st, Occurrence &o) {
  const bool general_locked = is_locked(st);
  if (general_locked)
    ImGui::BeginDisabled();

  int current = o.stat_key_id;
  std::string preview_label = o.stat_key;
  for (auto &k : st.stat_keys)
    if (k.id == current) {
      preview_label = stat_key_display_name(k);
      break;
    }

  ImGui::PushID(o.id ? o.id : (int)(intptr_t)&o);

  bool stat_checked = (o.stat_checked_in_game != 0);
  if (ImGui::Checkbox("Stat Checked In-Game", &stat_checked)) {
    o.stat_checked_in_game = stat_checked ? 1 : 0;
    mark_dirty(st);
  }

  const bool stat_locked = (o.stat_checked_in_game != 0);
  if (stat_locked)
    ImGui::BeginDisabled();

  if (ImGui::BeginCombo("Stat Key", preview_label.c_str())) {
    const int jump_ch = pressed_alpha_key_upper();
    bool jumped = false;

    for (auto &k : st.stat_keys) {
      bool sel = (k.id == current);
      std::string label = stat_key_display_name(k);
      if (ImGui::Selectable(label.c_str(), sel)) {
        o.stat_key_id = k.id;
        o.stat_key = k.name;
        mark_dirty(st);
      }
      if (sel)
        ImGui::SetItemDefaultFocus();
      if (!jumped && starts_with_alpha_ci(k.name, jump_ch)) {
        ImGui::SetScrollHereY(0.15f);
        ImGui::SetKeyboardFocusHere(-1);
        jumped = true;
      }
    }
    ImGui::EndCombo();
  }

  if (ImGui::InputDouble("Value", &o.value))
    mark_dirty(st);

  if (ImGui::Button("Delete")) {
    if (o.id > 0)
      st.deleted_occurrence_ids.push_back(o.id);
    o.id = -1;
    mark_dirty(st);
  }

  if (stat_locked)
    ImGui::EndDisabled();

  if (ImGui::TreeNode("Provenance")) {
    ImGui::Text("origin: %s", o.origin.c_str());
    if (o.generated_from_total_id.has_value())
      ImGui::Text("generated_from_total_id: %d", *o.generated_from_total_id);
    ImGui::Text("edited_by_user: %d", o.edited_by_user);
    ImGui::Text("stat_checked_in_game: %d", o.stat_checked_in_game);
    ImGui::Text("file: %s:%d", o.file_path.c_str(), o.line_number);
    ImGui::TextWrapped("raw: %s", o.raw_line.c_str());
    ImGui::TreePop();
  }

  ImGui::PopID();

  if (general_locked)
    ImGui::EndDisabled();
}

// ---------------------------
// PRETTY (READ-ONLY) STAT ROW
// ---------------------------
static void draw_stat_row_pretty(const Occurrence &o) {
  // Display value with up to 6 decimals.
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.6f", o.value);
  if (o.stat_checked_in_game != 0)
    ImGui::Text("[Checked] %s: %s", o.stat_key.c_str(), buf);
  else
    ImGui::Text("%s: %s", o.stat_key.c_str(), buf);
}

static int infer_is_total_from_pending(const PendingExample &p);
static bool delete_pending_group_for_context(Db &db, int pending_id,
                                             const std::string &context_type,
                                             int context_index);
static void remove_pending_group_from_state(EditorState &st, int pending_id,
                                            const std::string &context_type,
                                            int context_index);

static bool draw_inline_unresolved_rows(Db &db, EditorState &st,
                                        const std::string &context_type,
                                        int context_index, bool pretty,
                                        bool locked) {
  struct PendingGroup {
    int pending_id = 0;
    int display_id = 0;
    std::string raw_key;
    double display_value = 0.0;
  };

  std::vector<PendingGroup> groups;
  std::unordered_set<long long> seen;

  for (const auto &seed : st.pending) {
    if (seed.context_type != context_type)
      continue;
    const int idx = parse_first_int(seed.context_name);
    if (idx != context_index)
      continue;

    const long long dedupe_key =
        ((long long)seed.pending_id << 32) ^ (long long)(unsigned int)idx;
    if (!seen.insert(dedupe_key).second)
      continue;

    PendingGroup g{};
    g.pending_id = seed.pending_id;
    g.display_id = seed.id;
    g.raw_key = seed.raw_key;

    bool has_explicit_total = false;
    double specialty_sum = 0.0;
    std::optional<int> max_level;
    double max_level_value = seed.value;

    for (const auto &p : st.pending) {
      if (p.pending_id != seed.pending_id)
        continue;
      if (p.context_type != context_type)
        continue;
      if (parse_first_int(p.context_name) != context_index)
        continue;

      if (context_type == "Specialty") {
        specialty_sum += p.value;
        const int total_like = infer_is_total_from_pending(p);
        if (total_like == 1) {
          has_explicit_total = true;
          g.display_value = p.value;
          g.display_id = p.id;
        }
        if (p.level.has_value() &&
            (!max_level.has_value() || *p.level > *max_level)) {
          max_level = *p.level;
          max_level_value = p.value;
          g.display_id = p.id;
        }
      } else {
        g.display_value = p.value;
        g.display_id = p.id;
      }
    }

    if (context_type == "Specialty") {
      if (!has_explicit_total) {
        if (std::fabs(specialty_sum) > 0.0000001)
          g.display_value = specialty_sum; // synth-style groups
        else
          g.display_value = max_level_value;
      }
    }

    groups.push_back(std::move(g));
  }

  bool any_drawn = false;
  for (const auto &g : groups) {
    any_drawn = true;
    ImGui::PushID(g.display_id ? g.display_id : g.pending_id);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(245, 225, 110, 220));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(100, 90, 20, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));

    float h = pretty ? 58.0f : 86.0f;
    ImGui::BeginChild("##inline_unmapped", ImVec2(0, h), true);
    ImGui::Text("UNMAPPED: %s", g.raw_key.c_str());
    ImGui::Text("value: %.6f", g.display_value);

    if (!pretty) {
      if (locked)
        ImGui::BeginDisabled();
      if (ImGui::Button("Delete Unmapped Stat")) {
        bool ok = false;
        if (st.dirty) {
          st.pending_unmapped_delete_pending_id = g.pending_id;
          st.pending_unmapped_delete_context_type = context_type;
          st.pending_unmapped_delete_context_index = context_index;
          st.pending_unmapped_delete_raw_key = g.raw_key;
          st.show_unmapped_delete_modal = true;
        } else {
          ok = delete_pending_group_for_context(db, g.pending_id, context_type,
                                                context_index);
          if (ok) {
            remove_pending_group_from_state(st, g.pending_id, context_type,
                                            context_index);
          }
        }
        if (locked)
          ImGui::EndDisabled();
        ImGui::EndChild();
        ImGui::PopStyleColor(3);
        ImGui::PopID();
        return true;
      }
      if (locked)
        ImGui::EndDisabled();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(3);
    ImGui::PopID();
    ui_separator(1.0f);
  }

  return any_drawn;
}

// ---------------------------
// STATS PANEL (contents only)
// ---------------------------
static void draw_occurrences_contents(Db &db, EditorState &st) {
  if (st.selected_general_id <= 0) {
    ImGui::TextUnformatted("Select a general.");
    return;
  }

  // remove deleted
  st.occ.erase(std::remove_if(st.occ.begin(), st.occ.end(),
                              [](const Occurrence &o) { return o.id == -1; }),
               st.occ.end());

  const bool pretty = st.pretty_view;
  const bool locked = is_locked(st);

  // ---------------------------
  // 1) Base Skill
  // ---------------------------
  ui_section_bar("Base Skill", IM_COL32(70, 70, 70, 140));
  ui_separator(3.0f);

  for (auto &o : st.occ) {
    if (o.context_type == "BaseSkill") {
      if (pretty)
        draw_stat_row_pretty(o);
      else
        draw_stat_row(st, o);
    }
  }

  // BaseSkill unresolved keys have no numeric index; use 0 bucket.
  draw_inline_unresolved_rows(db, st, "BaseSkill", 0, pretty, locked);

  if (!pretty) {
    if (locked)
      ImGui::BeginDisabled();
    if (ImGui::Button("Add Base Skill")) {
      add_occurrence(st, "BaseSkill", st.meta.base_skill_name, std::nullopt, 0);
    }
    if (locked)
      ImGui::EndDisabled();
  }

  ui_separator(3.0f);

  // ---------------------------
  // 2) Ascensions 1..5
  // ---------------------------
  for (int asc = 1; asc <= 5; ++asc) {

    ImGui::Text("Ascension %d", asc);
    ui_separator(1.0f);
    // before: ImGui::Text("Ascension %d", asc);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Ascension %d", asc);
    ui_section_bar(buf, IM_COL32(60, 90, 160, 140));

    std::string ctx = make_ascension_name(asc);
    for (auto &o : st.occ) {
      if (ascension_index(o) == asc) {
        if (pretty)
          draw_stat_row_pretty(o);
        else
          draw_stat_row(st, o);
      }
    }

    draw_inline_unresolved_rows(db, st, "Ascension", asc, pretty, locked);

    if (!pretty) {
      if (locked)
        ImGui::BeginDisabled();
      std::string btn = "Add Ascension " + std::to_string(asc) + " Skill";
      if (ImGui::Button(btn.c_str())) {
        add_occurrence(st, "Ascension", ctx, std::nullopt, 0);
      }
      if (locked)
        ImGui::EndDisabled();
    }

    ui_separator(1.0f);
  }

  ui_separator(3.0f);

  // ---------------------------
  // 3) Specialties
  // ---------------------------
  int max_spec = 4;
  for (const auto &o : st.occ) {
    if (o.context_type != "Specialty")
      continue;
    max_spec = std::max(max_spec, specialty_index(o));
  }

  for (int spec_n = 1; spec_n <= max_spec; ++spec_n) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Specialty %d", spec_n);
    ui_section_bar(buf, IM_COL32(110, 70, 140, 140));

    ImGui::Text("Specialty %d", spec_n);
    ui_separator(1.0f);
    bool has_any_spec_rows = false;
    for (const auto &o : st.occ) {
      if (o.context_type == "Specialty" && specialty_index(o) == spec_n) {
        has_any_spec_rows = true;
        break;
      }
    }
    if (pretty && !has_any_spec_rows) {
      ImGui::TextDisabled("(none)");
      ui_separator(3.0f);
      continue;
    }

    auto has_total_for_key = [&](int stat_key_id) -> bool {
      for (const auto &o : st.occ) {
        if (o.context_type != "Specialty")
          continue;
        if (specialty_index(o) != spec_n)
          continue;
        if (!o.is_total)
          continue;
        if (o.stat_key_id == stat_key_id)
          return true;
      }
      return false;
    };

    // TOTAL rows first
    for (auto &total : st.occ) {
      if (total.context_type != "Specialty")
        continue;
      if (specialty_index(total) != spec_n)
        continue;
      if (!total.is_total)
        continue;

      ImGui::PushID(total.id ? total.id : (int)(intptr_t)&total);

      if (!pretty) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(160, 40, 40, 255));
        draw_stat_row(st, total);
        ImGui::PopStyleColor();
      } else {
        ImGui::TextUnformatted("TOTAL");
        draw_stat_row_pretty(total);
      }

      bool open =
          ImGui::TreeNodeEx("Levels", ImGuiTreeNodeFlags_SpanAvailWidth);

      if (!pretty) {
        ImGui::SameLine();
        if (locked)
          ImGui::BeginDisabled();
        if (ImGui::SmallButton("Expand (create missing)")) {
          expand_specialty_total(st, total);
        }
        if (locked)
          ImGui::EndDisabled();
      }

      if (open) {
        for (int lv = 1; lv <= 5; ++lv) {
          bool any = false;
          for (const auto &o : st.occ) {
            if (o.context_type != "Specialty")
              continue;
            if (specialty_index(o) != spec_n)
              continue;
            if (o.is_total)
              continue;
            if (!o.level.has_value())
              continue;
            if (*o.level != lv)
              continue;
            if (o.stat_key_id != total.stat_key_id)
              continue;
            any = true;
            break;
          }
          if (!any)
            continue;

          ImGui::Text("L%d", lv);
          ui_separator(1.0f);

          for (auto &o : st.occ) {
            if (o.context_type != "Specialty")
              continue;
            if (specialty_index(o) != spec_n)
              continue;
            if (o.is_total)
              continue;
            if (!o.level.has_value())
              continue;
            if (*o.level != lv)
              continue;
            if (o.stat_key_id != total.stat_key_id)
              continue;

            if (pretty)
              draw_stat_row_pretty(o);
            else
              draw_stat_row(st, o);
          }
        }
        ImGui::TreePop();
      }

      ImGui::PopID();
      ui_separator(1.0f);
    }

    // Rows not anchored by a TOTAL (common in older imported data).
    bool rendered_orphans_header = false;
    for (int lv = 1; lv <= 5; ++lv) {
      bool any = false;
      for (const auto &o : st.occ) {
        if (o.context_type != "Specialty")
          continue;
        if (specialty_index(o) != spec_n)
          continue;
        if (o.is_total)
          continue;
        if (!o.level.has_value())
          continue;
        if (*o.level != lv)
          continue;
        if (has_total_for_key(o.stat_key_id))
          continue;
        any = true;
        break;
      }
      if (!any)
        continue;

      if (!rendered_orphans_header) {
        ImGui::TextUnformatted("Rows Without TOTAL");
        rendered_orphans_header = true;
      }

      ImGui::Text("L%d", lv);
      ui_separator(1.0f);
      for (auto &o : st.occ) {
        if (o.context_type != "Specialty")
          continue;
        if (specialty_index(o) != spec_n)
          continue;
        if (o.is_total)
          continue;
        if (!o.level.has_value())
          continue;
        if (*o.level != lv)
          continue;
        if (has_total_for_key(o.stat_key_id))
          continue;
        if (pretty)
          draw_stat_row_pretty(o);
        else
          draw_stat_row(st, o);
      }
    }

    draw_inline_unresolved_rows(db, st, "Specialty", spec_n, pretty, locked);

    if (!pretty) {
      if (locked)
        ImGui::BeginDisabled();
      std::string add =
          "Add Specialty " + std::to_string(spec_n) + " Stat (TOTAL)";
      if (ImGui::Button(add.c_str())) {
        add_occurrence(st, "Specialty", make_specialty_name(spec_n, 5, true), 5,
                       1);
      }
      if (locked)
        ImGui::EndDisabled();
    }

    ui_separator(3.0f);
  }

  // ---------------------------
  // 4) Covenants (always show 1..6)
  // ---------------------------
  static const char *kCovenantNames[6] = {
      "War", "Cooperation", "Peace", "Faith", "Honor", "Civilization"};

  ImGui::TextUnformatted("Covenants");
  ui_separator(3.0f);

  for (int c_n = 1; c_n <= 6; ++c_n) {
    ImGui::Text("Covenant %d — %s", c_n, kCovenantNames[c_n - 1]);
    ui_separator(1.0f);

    bool any = false;
    for (auto &o : st.occ) {
      if (o.context_type != "Covenant")
        continue;
      if (covenant_index(o) != c_n)
        continue; // << key change: group by parsed index
      if (pretty)
        draw_stat_row_pretty(o);
      else
        draw_stat_row(st, o);
      any = true;
    }

    if (!any) {
      ImGui::TextDisabled("(none)");
    }

    std::string ctx = make_covenant_name(c_n); // "COVENANT N"
    std::string btn = "Add Covenant " + std::to_string(c_n) + " Stat";
    if (!pretty) {
      if (ImGui::Button(btn.c_str())) {
        add_occurrence(st, "Covenant", ctx, std::nullopt, 0);
      }
    }

    ui_separator(3.0f);
  }

  // Legacy imports may have non-indexed covenant contexts ("S", "COVENANTS").
  // Surface them so they can be corrected instead of silently disappearing.
  bool has_unindexed_covenants = false;
  for (const auto &o : st.occ) {
    if (o.context_type == "Covenant" && covenant_index(o) <= 0) {
      has_unindexed_covenants = true;
      break;
    }
  }
  if (has_unindexed_covenants) {
    ui_section_bar("Covenants (Unindexed Legacy Rows)",
                   IM_COL32(150, 95, 30, 160));
    ImGui::TextUnformatted(
        "These rows have non-standard covenant context names and are not tied "
        "to Covenant 1-6.");
    ui_separator(1.0f);

    for (auto &o : st.occ) {
      if (o.context_type != "Covenant")
        continue;
      if (covenant_index(o) > 0)
        continue;
      ImGui::Text("Context: %s", o.context_name.c_str());
      if (pretty)
        draw_stat_row_pretty(o);
      else
        draw_stat_row(st, o);
      ui_separator(1.0f);
    }
    ui_separator(3.0f);
  }
}
static int infer_is_total_from_pending(const PendingExample &p) {
  if (p.context_type != "Specialty" || !p.level.has_value() || *p.level != 5)
    return 0;

  if (p.raw_line.find("SYNTH_SPECIALTY_L5_INCREMENT") != std::string::npos)
    return 0;
  if (p.context_name.find("TOTAL") != std::string::npos)
    return 1;
  if (p.raw_line.find("TOTAL") != std::string::npos)
    return 1;
  if (p.context_name.find(" L") == std::string::npos)
    return 1;
  return 0;
}

static bool delete_pending_group_for_context(Db &db, int pending_id,
                                             const std::string &context_type,
                                             int context_index) {
  db.begin();
  try {
    DbStmt del(
        db,
        "DELETE FROM pending_stat_key_examples "
        "WHERE pending_id=?1 "
        "  AND context_type=?2 "
        "  AND CAST(TRIM(SUBSTR(context_name, INSTR(context_name, ' ') + 1)) "
        "AS INT)=?3;");
    del.bind_int(1, pending_id);
    del.bind_text(2, context_type.c_str());
    del.bind_int(3, context_index);
    del.st.step_done();

    DbStmt mark(
        db,
        "UPDATE pending_stat_keys "
        "SET status='mapped' "
        "WHERE id=?1 "
        "  AND NOT EXISTS(SELECT 1 FROM pending_stat_key_examples WHERE "
        "pending_id=?1);");
    mark.bind_int(1, pending_id);
    mark.st.step_done();

    db.commit();
    return true;
  } catch (...) {
    try {
      db.rollback();
    } catch (...) {
    }
    return false;
  }
}

static void remove_pending_group_from_state(EditorState &st, int pending_id,
                                            const std::string &context_type,
                                            int context_index) {
  st.pending.erase(
      std::remove_if(st.pending.begin(), st.pending.end(),
                     [&](const PendingExample &x) {
                       if (x.pending_id != pending_id)
                         return false;
                       if (x.context_type != context_type)
                         return false;
                       return parse_first_int(x.context_name) == context_index;
                     }),
      st.pending.end());
}

static void ensure_stat_checked_column(Db &db) {
  bool has_col = false;
  DbStmt st(db, "PRAGMA table_info(stat_occurrences);");
  while (st.step()) {
    const std::string col_name = st.col_text(1);
    if (col_name == "stat_checked_in_game") {
      has_col = true;
      break;
    }
  }
  if (!has_col) {
    db.exec("ALTER TABLE stat_occurrences "
            "ADD COLUMN stat_checked_in_game INTEGER NOT NULL DEFAULT 0;");
  }
}

static void ensure_origin_supports_gui(Db &db) {
  bool has_origin_col = false;
  {
    DbStmt st(db, "PRAGMA table_info(stat_occurrences);");
    while (st.step()) {
      const std::string col_name = st.col_text(1);
      if (col_name == "origin") {
        has_origin_col = true;
        break;
      }
    }
  }
  if (!has_origin_col)
    return;

  bool supports_gui = false;
  {
    DbStmt st(
        db,
        "SELECT sql FROM sqlite_master "
        "WHERE type='table' AND name='stat_occurrences';");
    if (st.step()) {
      const std::string sql = st.col_text(0);
      supports_gui = (sql.find("'gui'") != std::string::npos);
    }
  }
  if (supports_gui)
    return;

  db.exec("PRAGMA foreign_keys=OFF;");
  try {
    db.begin();
    db.exec(
        "CREATE TABLE stat_occurrences_new ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " general_id INTEGER NOT NULL,"
        " stat_key_id INTEGER NOT NULL,"
        " value REAL NOT NULL,"
        " context_type TEXT NOT NULL CHECK (context_type IN "
        "('BaseSkill','Ascension','Specialty','Covenant')),"
        " context_name TEXT NOT NULL,"
        " level INTEGER,"
        " is_total INTEGER NOT NULL DEFAULT 0,"
        " file_path TEXT NOT NULL,"
        " line_number INTEGER NOT NULL,"
        " raw_line TEXT NOT NULL,"
        " origin TEXT NOT NULL DEFAULT 'imported' CHECK (origin IN "
        "('imported','generated','gui')),"
        " generated_from_total_id INTEGER,"
        " edited_by_user INTEGER NOT NULL DEFAULT 0 CHECK (edited_by_user IN "
        "(0,1)),"
        " stat_checked_in_game INTEGER NOT NULL DEFAULT 0,"
        " FOREIGN KEY (general_id) REFERENCES generals(id),"
        " FOREIGN KEY (stat_key_id) REFERENCES stat_keys(id)"
        ");");

    db.exec(
        "INSERT INTO stat_occurrences_new("
        " id, general_id, stat_key_id, value, context_type, context_name,"
        " level, is_total, file_path, line_number, raw_line,"
        " origin, generated_from_total_id, edited_by_user, stat_checked_in_game"
        ") "
        "SELECT "
        " id, general_id, stat_key_id, value, context_type, context_name,"
        " level, is_total, file_path, line_number, raw_line,"
        " CASE WHEN origin='manual' THEN 'gui' ELSE origin END,"
        " generated_from_total_id, edited_by_user, COALESCE(stat_checked_in_game,0)"
        " FROM stat_occurrences;");

    db.exec("DROP TABLE stat_occurrences;");
    db.exec("ALTER TABLE stat_occurrences_new RENAME TO stat_occurrences;");
    db.exec(
        "CREATE INDEX idx_stat_occ_general ON stat_occurrences(general_id);");
    db.exec(
        "CREATE INDEX idx_stat_occ_statkey ON stat_occurrences(stat_key_id);");
    db.commit();
  } catch (...) {
    try {
      db.rollback();
    } catch (...) {
    }
    db.exec("PRAGMA foreign_keys=ON;");
    throw;
  }

  db.exec("PRAGMA foreign_keys=ON;");
}

void ui_init(Db &db, EditorState &st) {
  ensure_stat_checked_column(db);
  ensure_origin_supports_gui(db);
  db_normalize_general_roles(db);
  st.filter_role = "All";
  st.list = db_load_general_list(db, st.filter_role, st.filter_name);
  st.stat_keys = db_load_stat_keys(db);
}

void ui_draw(Db &db, EditorState &st) {
  // Dirty modal (unchanged)
  if (st.show_dirty_modal) {
    ImGui::OpenPopup("Unsaved changes");
    st.show_dirty_modal = false;
  }

  if (ImGui::BeginPopupModal("Unsaved changes", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
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

  if (st.show_unmapped_delete_modal) {
    ImGui::OpenPopup("Delete Unmapped Stat");
    st.show_unmapped_delete_modal = false;
  }

  if (ImGui::BeginPopupModal("Delete Unmapped Stat", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(
        "You have unsaved edits. Deleting an unmapped stat now can lose them.");
    if (!st.pending_unmapped_delete_raw_key.empty()) {
      ImGui::Text("Unmapped key: %s",
                  st.pending_unmapped_delete_raw_key.c_str());
    }
    ImGui::Spacing();
    ImGui::TextUnformatted("Save before deleting?");

    if (ImGui::Button("Save + Delete")) {
      if (save_changes(db, st)) {
        if (delete_pending_group_for_context(
                db, st.pending_unmapped_delete_pending_id,
                st.pending_unmapped_delete_context_type,
                st.pending_unmapped_delete_context_index)) {
          remove_pending_group_from_state(
              st, st.pending_unmapped_delete_pending_id,
              st.pending_unmapped_delete_context_type,
              st.pending_unmapped_delete_context_index);
          st.list = db_load_general_list(db, st.filter_role, st.filter_name);
        }
      }
      st.pending_unmapped_delete_pending_id = 0;
      st.pending_unmapped_delete_context_type.clear();
      st.pending_unmapped_delete_context_index = 0;
      st.pending_unmapped_delete_raw_key.clear();
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete Without Save")) {
      if (delete_pending_group_for_context(
              db, st.pending_unmapped_delete_pending_id,
              st.pending_unmapped_delete_context_type,
              st.pending_unmapped_delete_context_index)) {
        remove_pending_group_from_state(
            st, st.pending_unmapped_delete_pending_id,
            st.pending_unmapped_delete_context_type,
            st.pending_unmapped_delete_context_index);
        st.list = db_load_general_list(db, st.filter_role, st.filter_name);
      }
      st.pending_unmapped_delete_pending_id = 0;
      st.pending_unmapped_delete_context_type.clear();
      st.pending_unmapped_delete_context_index = 0;
      st.pending_unmapped_delete_raw_key.clear();
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      st.pending_unmapped_delete_pending_id = 0;
      st.pending_unmapped_delete_context_type.clear();
      st.pending_unmapped_delete_context_index = 0;
      st.pending_unmapped_delete_raw_key.clear();
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  // ============================================================
  // SINGLE WORKSPACE WINDOW (prevents floating windows)
  // ============================================================
  const ImGuiViewport *vp = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(vp->Pos);
  ImGui::SetNextWindowSize(vp->Size);
  // ImGui::SetNextWindowViewport(vp->ID);

  const ImGuiWindowFlags root_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_MenuBar;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

  if (ImGui::Begin("##workspace", nullptr, root_flags)) {
    if (ImGui::BeginMenuBar()) {
      ImGui::TextUnformatted("Evony General Analyzer");
      ImGui::EndMenuBar();
    }

    const float left_w = 280.0f;
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
    ImGui::BeginChild("##stats", ImVec2(0, 0), true);
    ImGui::TextUnformatted("Stats");
    ImGui::Separator();
    draw_occurrences_contents(db, st);
    ImGui::EndChild();

    ImGui::EndChild(); // bottom
    ImGui::EndChild(); // right
  }

  ImGui::End();
  ImGui::PopStyleVar(3);
}

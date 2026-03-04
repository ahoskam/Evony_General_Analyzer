#include "model.h"
#include "model_api.h"
#include "db.h"
#include "role_utils.h"

#include <string>
#include <vector>
#include <optional>

std::vector<GeneralRow> db_load_general_list(Db& db, const std::string& role_filter, const std::string& name_like)
{
  // Local flag bits (use these same values in UI)
  enum GeneralStatusFlags : int {
    GS_MISSING_SOURCE_TEXT   = 1 << 0,
    GS_MISSING_BASE_SKILL    = 1 << 1,
    GS_MISSING_ASCENSIONS    = 1 << 2,
    GS_MISSING_SPECIALTIES   = 1 << 3,
    GS_MISSING_COVENANT_6    = 1 << 4,
  };

  std::string sql =
    "SELECT "
    "  g.id, "
    "  g.name, "
    "  g.role, "
    "  g.double_checked_in_game, "
    "  CASE WHEN g.source_text_verbatim IS NOT NULL AND LENGTH(TRIM(g.source_text_verbatim)) > 0 THEN 1 ELSE 0 END AS has_source, "
    "  (SELECT COUNT(1) FROM stat_occurrences s "
    "     WHERE s.general_id = g.id AND s.context_type = 'BaseSkill') AS base_cnt, "
    "  (SELECT COUNT(DISTINCT s.context_name) FROM stat_occurrences s "
    "     WHERE s.general_id = g.id AND s.context_type = 'Ascension') AS asc_cnt, "
    "  (SELECT COUNT(DISTINCT CAST(TRIM(SUBSTR(s.context_name, INSTR(s.context_name, ' ') + 1)) AS INT)) "
    "     FROM stat_occurrences s "
    "     WHERE s.general_id = g.id AND s.context_type = 'Specialty' "
    "       AND CAST(TRIM(SUBSTR(s.context_name, INSTR(s.context_name, ' ') + 1)) AS INT) > 0) AS spec_cnt, "
    "  COALESCE((SELECT MAX(CAST(TRIM(SUBSTR(s.context_name, 10)) AS INT)) FROM stat_occurrences s "
    "     WHERE s.general_id = g.id AND s.context_type = 'Covenant' "
    "       AND (s.context_name LIKE 'COVENANT %' OR s.context_name LIKE 'Covenant %')), 0) AS cov_max "
    "FROM generals g "
    "WHERE 1=1 ";

  int bind = 1;
  bool has_role = (!role_filter.empty() && role_filter != "All");
  std::string normalized_role_filter;
  if (has_role) {
    normalized_role_filter = normalize_general_role(role_filter);
  }
  bool has_like = (!name_like.empty());

  if (has_role) sql += "AND g.role=?1 ";
  if (has_like) sql += std::string("AND g.name LIKE ?") + std::to_string(has_role ? 2 : 1) + " ";
  sql += "ORDER BY g.name;";

  DbStmt st(db, sql.c_str());
  if (has_role) st.bind_text(bind++, normalized_role_filter.c_str());
  if (has_like) {
    std::string like = "%" + name_like + "%";
    st.bind_text(bind++, like.c_str());
  }

  std::vector<GeneralRow> out;
  while (st.step()) {
    GeneralRow g{};
    g.id = st.col_int(0);
    g.name = st.col_text(1);
    g.role = normalize_general_role(st.col_text(2));
    g.double_checked_in_game = st.col_int(3);

    const int has_source = st.col_int(4);
    const int base_cnt   = st.col_int(5);
    const int asc_cnt    = st.col_int(6);
    const int spec_cnt   = st.col_int(7);
    const int cov_max    = st.col_int(8);

    int flags = 0;
    if (!has_source)      flags |= GS_MISSING_SOURCE_TEXT;
    if (base_cnt <= 0)    flags |= GS_MISSING_BASE_SKILL;
    if (asc_cnt < 5)      flags |= GS_MISSING_ASCENSIONS;
    if (spec_cnt < 4)     flags |= GS_MISSING_SPECIALTIES;
    if (cov_max >= 1 && cov_max < 6) flags |= GS_MISSING_COVENANT_6;

    g.status_flags = flags;
    g.covenant_max = cov_max;

    out.push_back(std::move(g));
  }
  return out;
}


std::vector<StatKey> db_load_stat_keys(Db& db)
{
  DbStmt st(db, "SELECT id, key FROM stat_keys ORDER BY key;");
  std::vector<StatKey> out;
  while (st.step()) {
    StatKey k{};
    k.id = st.col_int(0);
    k.name = st.col_text(1);
    out.push_back(std::move(k));
  }
  return out;
}

GeneralMeta db_load_general_meta(Db& db, int id)
{
  GeneralMeta m{};
  DbStmt st(db,
    "SELECT id, name, role, in_tavern, base_skill_name,"
    " leadership, leadership_green, attack, attack_green, defense, defense_green, politics, politics_green,"
    " role_confirmed, source_text_verbatim, double_checked_in_game "
    "FROM generals WHERE id=?1;");
  st.bind_int(1, id);

  if (st.step()) {
    m.id = st.col_int(0);
    m.name = st.col_text(1);
    m.role = normalize_general_role(st.col_text(2));
    m.in_tavern = st.col_int(3);
    m.base_skill_name = st.col_text(4);

    m.leadership = st.col_int(5);
    m.leadership_green = st.col_double(6);
    m.attack = st.col_int(7);
    m.attack_green = st.col_double(8);
    m.defense = st.col_int(9);
    m.defense_green = st.col_double(10);
    m.politics = st.col_int(11);
    m.politics_green = st.col_double(12);

    m.role_confirmed = st.col_int(13);
    m.source_text_verbatim = st.col_text(14);
    m.double_checked_in_game = st.col_int(15);
  }
  return m;
}

std::vector<Occurrence> db_load_occurrences(Db& db, int general_id)
{
  DbStmt st(db,
    "SELECT "
    " o.id, o.general_id, o.stat_key_id, k.key, o.value, "
    " o.context_type, o.context_name, o.level, o.is_total, "
    " o.file_path, o.line_number, o.raw_line, "
    " o.origin, o.generated_from_total_id, o.edited_by_user, "
    " o.stat_checked_in_game "
    "FROM stat_occurrences o "
    "JOIN stat_keys k ON k.id=o.stat_key_id "
    "WHERE o.general_id=?1 "
    "ORDER BY o.context_type, o.context_name, o.level, o.is_total DESC, k.key;");
  st.bind_int(1, general_id);

  std::vector<Occurrence> out;
  while (st.step()) {
    Occurrence o{};
    o.id = st.col_int(0);
    o.general_id = st.col_int(1);
    o.stat_key_id = st.col_int(2);
    o.stat_key = st.col_text(3);
    o.value = st.col_double(4);

    o.context_type = st.col_text(5);
    o.context_name = st.col_text(6);

    if (!st.col_is_null(7)) o.level = st.col_int(7);
    o.is_total = st.col_int(8);

    o.file_path = st.col_text(9);
    o.line_number = st.col_int(10);
    o.raw_line = st.col_text(11);

    o.origin = st.col_text(12);
    if (!st.col_is_null(13)) o.generated_from_total_id = st.col_int(13);
    o.edited_by_user = st.col_int(14);
    o.stat_checked_in_game = st.col_int(15);

    out.push_back(std::move(o));
  }
  return out;
}

std::vector<PendingExample> db_load_pending_examples_for_general(Db& db, const std::string& general_name)
{
  DbStmt st(db,
    "SELECT "
    " e.id, e.pending_id, p.raw_key, e.value, "
    " e.context_type, e.context_name, e.level, "
    " e.file_path, e.line_number, e.raw_line "
    "FROM pending_stat_key_examples e "
    "JOIN pending_stat_keys p ON p.id=e.pending_id "
    "WHERE e.general_name=?1 "
    "ORDER BY p.raw_key, e.file_path, e.line_number;");
  st.bind_text(1, general_name.c_str());

  std::vector<PendingExample> out;
  while (st.step()) {
    PendingExample p{};
    p.id = st.col_int(0);
    p.pending_id = st.col_int(1);
    p.raw_key = st.col_text(2);
    p.value = st.col_double(3);
    p.context_type = st.col_text(4);
    p.context_name = st.col_text(5);
    if (!st.col_is_null(6)) p.level = st.col_int(6);
    p.file_path = st.col_text(7);
    p.line_number = st.col_int(8);
    p.raw_line = st.col_text(9);
    out.push_back(std::move(p));
  }
  return out;
}

LoadAllResult db_load_all_for_general(Db& db, int general_id)
{
  LoadAllResult r;
  r.meta = db_load_general_meta(db, general_id);
  r.occ = db_load_occurrences(db, general_id);
  r.pending = db_load_pending_examples_for_general(db, r.meta.name);
  return r;
}

void db_normalize_general_roles(Db& db)
{
  db.exec(
    "DROP TRIGGER IF EXISTS trg_generals_role_valid_insert;"
    "DROP TRIGGER IF EXISTS trg_generals_role_valid_update;"
    "CREATE TRIGGER trg_generals_role_valid_insert "
    "BEFORE INSERT ON generals "
    "FOR EACH ROW "
    "BEGIN "
    "  SELECT CASE "
    "    WHEN NEW.role NOT IN ('Ground','Mounted','Ranged','Siege','Defense','Mixed','Admin','Duty','Mayor','Unknown') "
    "    THEN RAISE(ABORT, 'Invalid generals.role') "
    "  END; "
    "END;"
    "CREATE TRIGGER trg_generals_role_valid_update "
    "BEFORE UPDATE OF role ON generals "
    "FOR EACH ROW "
    "BEGIN "
    "  SELECT CASE "
    "    WHEN NEW.role NOT IN ('Ground','Mounted','Ranged','Siege','Defense','Mixed','Admin','Duty','Mayor','Unknown') "
    "    THEN RAISE(ABORT, 'Invalid generals.role') "
    "  END; "
    "END;"
  );

  db.exec(
    "UPDATE generals "
    "SET role = CASE "
    "  WHEN LOWER(TRIM(role)) LIKE 'admin%' THEN 'Admin' "
    "  WHEN LOWER(TRIM(role))='ground' THEN 'Ground' "
    "  WHEN LOWER(TRIM(role))='mounted' THEN 'Mounted' "
    "  WHEN LOWER(TRIM(role))='ranged' THEN 'Ranged' "
    "  WHEN LOWER(TRIM(role))='siege' THEN 'Siege' "
    "  WHEN LOWER(TRIM(role))='defense' THEN 'Defense' "
    "  WHEN LOWER(TRIM(role))='mixed' THEN 'Mixed' "
    "  WHEN LOWER(TRIM(role))='duty' THEN 'Duty' "
    "  WHEN LOWER(TRIM(role))='mayor' THEN 'Mayor' "
    "  WHEN LOWER(TRIM(role))='unknown' THEN 'Unknown' "
    "  ELSE 'Unknown' "
    "END "
    "WHERE role <> CASE "
    "  WHEN LOWER(TRIM(role)) LIKE 'admin%' THEN 'Admin' "
    "  WHEN LOWER(TRIM(role))='ground' THEN 'Ground' "
    "  WHEN LOWER(TRIM(role))='mounted' THEN 'Mounted' "
    "  WHEN LOWER(TRIM(role))='ranged' THEN 'Ranged' "
    "  WHEN LOWER(TRIM(role))='siege' THEN 'Siege' "
    "  WHEN LOWER(TRIM(role))='defense' THEN 'Defense' "
    "  WHEN LOWER(TRIM(role))='mixed' THEN 'Mixed' "
    "  WHEN LOWER(TRIM(role))='duty' THEN 'Duty' "
    "  WHEN LOWER(TRIM(role))='mayor' THEN 'Mayor' "
    "  WHEN LOWER(TRIM(role))='unknown' THEN 'Unknown' "
    "  ELSE 'Unknown' "
    "END;"
  );
}
// -------------------------
// WRITE API (needed by UI)
// -------------------------
#include <iostream>

static void bind_text(sqlite3_stmt* s, int idx, const std::string& v) {
  sqlite3_bind_text(s, idx, v.c_str(), -1, SQLITE_TRANSIENT);
}
static void bind_int(sqlite3_stmt* s, int idx, int v) {
  sqlite3_bind_int(s, idx, v);
}
static void bind_double(sqlite3_stmt* s, int idx, double v) {
  sqlite3_bind_double(s, idx, v);
}
static void bind_null(sqlite3_stmt* s, int idx) {
  sqlite3_bind_null(s, idx);
}

bool db_update_general_meta(Db& db, int general_id, const GeneralMeta& g)
{
  try {
    const std::string normalized_role = normalize_general_role(g.role);
    auto st = db.prepare(
      "UPDATE generals SET "
      " role=?1, in_tavern=?2, base_skill_name=?3, "
      " leadership=?4, leadership_green=?5, "
      " attack=?6, attack_green=?7, "
      " defense=?8, defense_green=?9, "
      " politics=?10, politics_green=?11, "
      " role_confirmed=?12, source_text_verbatim=?13, double_checked_in_game=?14 "
      "WHERE id=?15;"
    );

    bind_text(st.stmt, 1, normalized_role);
    bind_int (st.stmt, 2, g.in_tavern);
    bind_text(st.stmt, 3, g.base_skill_name);

    bind_int   (st.stmt, 4, g.leadership);
    bind_double(st.stmt, 5, g.leadership_green);

    bind_int   (st.stmt, 6, g.attack);
    bind_double(st.stmt, 7, g.attack_green);

    bind_int   (st.stmt, 8, g.defense);
    bind_double(st.stmt, 9, g.defense_green);

    bind_int   (st.stmt, 10, g.politics);
    bind_double(st.stmt, 11, g.politics_green);

    bind_int (st.stmt, 12, g.role_confirmed);
    bind_text(st.stmt, 13, g.source_text_verbatim);
    bind_int (st.stmt, 14, g.double_checked_in_game);

    bind_int(st.stmt, 15, general_id);

    st.step_done();
    return true;
  } catch (const std::exception& e) {
    std::cerr << "db_update_general_meta failed: " << e.what() << "\n";
    return false;
  }
}

void db_delete_occurrence(Db& db, int id)
{
  auto st = db.prepare("DELETE FROM stat_occurrences WHERE id=?1;");
  bind_int(st.stmt, 1, id);
  st.step_done();
}

int db_insert_occurrence(Db& db, Occurrence& o)
{
  auto st = db.prepare(
    "INSERT INTO stat_occurrences("
    " general_id, stat_key_id, value, "
    " context_type, context_name, level, is_total, "
    " file_path, line_number, raw_line, "
    " origin, generated_from_total_id, edited_by_user, "
    " stat_checked_in_game"
    ") VALUES("
    " ?1, ?2, ?3, "
    " ?4, ?5, ?6, ?7, "
    " ?8, ?9, ?10, "
    " ?11, ?12, ?13, "
    " ?14"
    ");"
  );

  bind_int(st.stmt, 1, o.general_id);
  bind_int(st.stmt, 2, o.stat_key_id);
  bind_double(st.stmt, 3, o.value);

  bind_text(st.stmt, 4, o.context_type);
  bind_text(st.stmt, 5, o.context_name);

  if (o.level.has_value()) bind_int(st.stmt, 6, *o.level);
  else bind_null(st.stmt, 6);

  bind_int(st.stmt, 7, o.is_total);

  bind_text(st.stmt, 8, o.file_path);
  bind_int (st.stmt, 9, o.line_number);
  bind_text(st.stmt, 10, o.raw_line);

  bind_text(st.stmt, 11, o.origin);

  if (o.generated_from_total_id.has_value()) bind_int(st.stmt, 12, *o.generated_from_total_id);
  else bind_null(st.stmt, 12);

  bind_int(st.stmt, 13, o.edited_by_user);
  bind_int(st.stmt, 14, o.stat_checked_in_game);

  st.step_done();

  o.id = (int)sqlite3_last_insert_rowid(db.raw());
  return o.id;
}

void db_update_occurrence(Db& db, const Occurrence& o)
{
  auto st = db.prepare(
    "UPDATE stat_occurrences SET "
    " stat_key_id=?1, value=?2, "
    " context_type=?3, context_name=?4, level=?5, is_total=?6, "
    " file_path=?7, line_number=?8, raw_line=?9, "
    " origin=?10, generated_from_total_id=?11, edited_by_user=?12, "
    " stat_checked_in_game=?13 "
    "WHERE id=?14;"
  );

  bind_int(st.stmt, 1, o.stat_key_id);
  bind_double(st.stmt, 2, o.value);

  bind_text(st.stmt, 3, o.context_type);
  bind_text(st.stmt, 4, o.context_name);

  if (o.level.has_value()) bind_int(st.stmt, 5, *o.level);
  else bind_null(st.stmt, 5);

  bind_int(st.stmt, 6, o.is_total);

  bind_text(st.stmt, 7, o.file_path);
  bind_int (st.stmt, 8, o.line_number);
  bind_text(st.stmt, 9, o.raw_line);

  bind_text(st.stmt, 10, o.origin);

  if (o.generated_from_total_id.has_value()) bind_int(st.stmt, 11, *o.generated_from_total_id);
  else bind_null(st.stmt, 11);

  bind_int(st.stmt, 12, o.edited_by_user);
  bind_int(st.stmt, 13, o.stat_checked_in_game);
  bind_int(st.stmt, 14, o.id);

  st.step_done();
}

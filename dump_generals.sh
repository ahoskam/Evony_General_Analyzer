#!/usr/bin/env bash
set -euo pipefail

DB="${1:-data/evony_v2.db}"
OUTDIR="${2:-data/exported}"

if [ ! -f "$DB" ]; then
  echo "Database not found: $DB"
  exit 1
fi

mkdir -p "$OUTDIR"

# Make filenames safe-ish (keep letters/numbers/_/- only, convert spaces)
slugify() {
  echo "$1" | sed -E 's/[[:space:]]+/_/g; s/[^A-Za-z0-9_.-]//g'
}

# Escape for SQL single-quoted literal: O'Brien -> O''Brien
sql_escape() {
  echo "$1" | sed "s/'/''/g"
}

mapfile -t NAMES < <(sqlite3 -noheader "$DB" "SELECT name FROM generals ORDER BY name;")

echo "Exporting ${#NAMES[@]} generals to $OUTDIR"
echo

COUNT=0

for NAME in "${NAMES[@]}"; do
  NAME_SQL="$(sql_escape "$NAME")"
  FILE="$OUTDIR/$(slugify "$NAME").txt"

  # ----------------------------
  # Metadata + core + flags
  # ----------------------------
  sqlite3 -noheader -separator $'\t' "$DB" "
  SELECT
    name,
    role,
    in_tavern,
    base_skill_name,
    leadership, printf('%.2f', leadership_green),
    attack,     printf('%.2f', attack_green),
    defense,    printf('%.2f', defense_green),
    politics,   printf('%.2f', politics_green),
    role_confirmed,
    double_checked_in_game
  FROM generals
  WHERE name = '$NAME_SQL';
  " | while IFS=$'\t' read -r name role in_tavern base_skill_name \
                           leadership leadership_green \
                           attack attack_green \
                           defense defense_green \
                           politics politics_green \
                           role_confirmed double_checked; do
        cat >"$FILE" <<EOF
GENERAL: $name
ROLE: $role
IN_TAVERN: $([ "$in_tavern" = "1" ] && echo "true" || echo "false")
BASE SKILL | $base_skill_name
CORE:
  Leadership: $leadership ($leadership_green)
  Attack: $attack ($attack_green)
  Defense: $defense ($defense_green)
  Politics: $politics ($politics_green)
FLAGS:
  role_confirmed: $role_confirmed
  double_checked_in_game: $double_checked

EOF
  done

  # ----------------------------
  # Stats, grouped by context
  # ----------------------------
  sqlite3 -noheader -separator $'\t' "$DB" "
  WITH occ AS (
    SELECT
      o.context_type,
      o.context_name,
      COALESCE(o.level, -1) AS lvl,
      o.is_total,
      sk.key AS stat_key,
      o.value
    FROM stat_occurrences o
    JOIN generals g ON g.id=o.general_id
    JOIN stat_keys sk ON sk.id=o.stat_key_id
    WHERE g.name = '$NAME_SQL'
  )
  SELECT context_type, context_name, lvl, is_total, stat_key, value
  FROM occ
  ORDER BY
    CASE context_type
      WHEN 'BaseSkill' THEN 1
      WHEN 'Ascension' THEN 2
      WHEN 'Specialty' THEN 3
      WHEN 'Covenant' THEN 4
      ELSE 9
    END,
    context_name,
    CASE WHEN lvl=-1 THEN 999 ELSE lvl END,
    stat_key;
  " | awk -F'\t' '
    BEGIN { cur_ct=""; cur_cn=""; cur_lvl=""; }

    function print_header(ct, cn, lvl) {
      if (ct=="BaseSkill") {
        print "";
        print "==============================";
        print "BASE SKILL STATS";
        print "==============================";
        return;
      }
      if (ct=="Ascension") {
        print "";
        print "==============================";
        print "ASCENSION " lvl;
        print "==============================";
        return;
      }
      if (ct=="Specialty") {
        print "";
        print "==============================";
        if (lvl==5) print cn " L5 (TOTAL)";
        else        print cn " L" lvl;
        print "==============================";
        return;
      }
      if (ct=="Covenant") {
        print "";
        print "==============================";
        print "COVENANT | " cn;
        print "==============================";
        return;
      }
      print "";
      print "==============================";
      print ct " | " cn;
      print "==============================";
    }

    {
      ct=$1; cn=$2; lvl=$3; key=$5; val=$6;
      shown_lvl = (lvl==-1 ? "" : lvl);

      if (ct!=cur_ct || cn!=cur_cn || shown_lvl!=cur_lvl) {
        cur_ct=ct; cur_cn=cn; cur_lvl=shown_lvl;
        print_header(ct, cn, (shown_lvl==""? "" : shown_lvl));
      }

      sign = (val+0 >= 0 ? "+" : "");
      printf("%s %s%s\n", key, sign, val);
    }
  ' >> "$FILE"

  # ----------------------------
  # SOURCE TEXT (VERBATIM)
  # (Print header ONCE; store may already contain it)
  # ----------------------------
  {
    echo ""
    echo "=============================="
    echo "# SOURCE TEXT (VERBATIM)"
    echo "=============================="
  } >> "$FILE"

  sqlite3 -noheader "$DB" "
  SELECT COALESCE(source_text_verbatim,'')
  FROM generals
  WHERE name = '$NAME_SQL';
  " | awk '
    BEGIN { skipdone=0 }
    # If stored text begins with the same 3-line header, skip it once:
    skipdone==0 && $0=="==============================" { getline; getline; getline; skipdone=1; next }
    { print; skipdone=1 }
  ' >> "$FILE"

  COUNT=$((COUNT+1))
done

echo
echo "Export complete."
echo "$COUNT files written to $OUTDIR"
echo
echo "WARNING: Pressing Enter will DELETE: $OUTDIR"
echo

read -r -p "Type 'K' to keep exported files or press Enter to delete them: " ANSWER

if [[ "$ANSWER" == "K" || "$ANSWER" == "k" ]]; then
  echo "Keeping exported files in: $OUTDIR"
else
  echo "Deleting exported files..."
  rm -rf "$OUTDIR"
  echo "Deleted: $OUTDIR"
fi

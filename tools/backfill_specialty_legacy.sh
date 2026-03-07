#!/usr/bin/env bash
set -euo pipefail

DB_PATH="${1:-data/evony_v2.db}"

if [[ ! -f "$DB_PATH" ]]; then
  echo "Database not found: $DB_PATH" >&2
  exit 1
fi

backup_path="${DB_PATH}.bak_pre_specialty_backfill_$(date +%Y%m%d_%H%M%S).db"
cp "$DB_PATH" "$backup_path"
echo "Backup created: $backup_path"

sqlite3 "$DB_PATH" <<'SQL'
BEGIN TRANSACTION;

DROP TABLE IF EXISTS _sp_patterns;
CREATE TEMP TABLE _sp_patterns (
  total_abs INTEGER PRIMARY KEY,
  l1 INTEGER NOT NULL,
  l2 INTEGER NOT NULL,
  l3 INTEGER NOT NULL,
  l4 INTEGER NOT NULL,
  l5 INTEGER NOT NULL
);

INSERT INTO _sp_patterns(total_abs, l1, l2, l3, l4, l5) VALUES
  (6,  1, 1, 1, 1, 2),
  (10, 1, 1, 2, 2, 4),
  (15, 1, 2, 3, 3, 6),
  (16, 2, 2, 3, 3, 6),
  (20, 2, 2, 4, 4, 8),
  (25, 2, 3, 5, 5, 10),
  (26, 3, 3, 5, 5, 10),
  (30, 3, 3, 6, 6, 12),
  (35, 3, 4, 6, 7, 15),
  (36, 4, 4, 7, 7, 14),
  (40, 4, 4, 8, 8, 16),
  (45, 4, 5, 9, 9, 18),
  (46, 5, 5, 9, 9, 18),
  (50, 5, 5, 10, 10, 20);

DROP TABLE IF EXISTS _sp_targets;
CREATE TEMP TABLE _sp_targets AS
SELECT
  t.id AS total_id,
  t.general_id,
  t.stat_key_id,
  CAST(TRIM(SUBSTR(t.context_name, 10)) AS INTEGER) AS spec_n,
  t.value AS total_value,
  CAST(ROUND(ABS(t.value)) AS INTEGER) AS abs_total,
  CASE WHEN t.value < 0 THEN -1.0 ELSE 1.0 END AS sign
FROM stat_occurrences t
JOIN _sp_patterns p
  ON p.total_abs = CAST(ROUND(ABS(t.value)) AS INTEGER)
WHERE t.context_type = 'Specialty'
  AND t.is_total = 1
  AND t.level = 5
  AND UPPER(t.context_name) LIKE 'SPECIALTY %'
  AND CAST(TRIM(SUBSTR(t.context_name, 10)) AS INTEGER) > 0;

DROP TABLE IF EXISTS _sp_eligible;
CREATE TEMP TABLE _sp_eligible AS
SELECT *
FROM _sp_targets tgt
WHERE NOT EXISTS (
  SELECT 1
  FROM stat_occurrences x
  WHERE x.general_id = tgt.general_id
    AND x.stat_key_id = tgt.stat_key_id
    AND x.context_type = 'Specialty'
    AND x.is_total = 0
    AND x.level BETWEEN 1 AND 4
    AND CAST(TRIM(SUBSTR(x.context_name, 10)) AS INTEGER) = tgt.spec_n
);

DROP TABLE IF EXISTS _sp_singleton_l5;
CREATE TEMP TABLE _sp_singleton_l5 AS
SELECT
  e.total_id,
  e.general_id,
  e.stat_key_id,
  e.spec_n,
  e.abs_total,
  e.sign,
  p.l5,
  MIN(l5.id) AS l5_id,
  COUNT(*) AS l5_count,
  MIN(ABS(l5.value)) AS min_abs_l5,
  MAX(ABS(l5.value)) AS max_abs_l5
FROM _sp_eligible e
JOIN _sp_patterns p
  ON p.total_abs = e.abs_total
JOIN stat_occurrences l5
  ON l5.general_id = e.general_id
 AND l5.stat_key_id = e.stat_key_id
 AND l5.context_type = 'Specialty'
 AND l5.is_total = 0
 AND l5.level = 5
 AND CAST(TRIM(SUBSTR(l5.context_name, 10)) AS INTEGER) = e.spec_n
GROUP BY e.total_id, e.general_id, e.stat_key_id, e.spec_n, e.abs_total, e.sign, p.l5
HAVING l5_count = 1
   AND ABS(min_abs_l5 - abs_total) < 0.0000001
   AND ABS(max_abs_l5 - abs_total) < 0.0000001;

UPDATE stat_occurrences
SET
  value = (
    SELECT s.sign * s.l5
    FROM _sp_singleton_l5 s
    WHERE s.l5_id = stat_occurrences.id
  ),
  context_name = (
    'SPECIALTY ' || (
      SELECT s.spec_n FROM _sp_singleton_l5 s WHERE s.l5_id = stat_occurrences.id
    ) || ' L5'
  ),
  origin = 'generated',
  generated_from_total_id = COALESCE(
    generated_from_total_id,
    (SELECT s.total_id FROM _sp_singleton_l5 s WHERE s.l5_id = stat_occurrences.id)
  ),
  raw_line = 'MIGRATE_SINGLETON_L5_TO_INCREMENT'
WHERE id IN (SELECT l5_id FROM _sp_singleton_l5);

COMMIT;
SQL

# Build deterministic insert statements from eligible TOTAL rows.
rows_tsv=$(sqlite3 -separator '|' "$DB_PATH" "
WITH patterns(total_abs,l1,l2,l3,l4,l5) AS (
  VALUES
    (6,1,1,1,1,2),
    (10,1,1,2,2,4),
    (15,1,2,3,3,6),
    (16,2,2,3,3,6),
    (20,2,2,4,4,8),
    (25,2,3,5,5,10),
    (26,3,3,5,5,10),
    (30,3,3,6,6,12),
    (35,3,4,6,7,15),
    (36,4,4,7,7,14),
    (40,4,4,8,8,16),
    (45,4,5,9,9,18),
    (46,5,5,9,9,18),
    (50,5,5,10,10,20)
),
targets AS (
  SELECT
    t.id AS total_id,
    t.general_id,
    t.stat_key_id,
    CAST(TRIM(SUBSTR(t.context_name, 10)) AS INTEGER) AS spec_n,
    t.value AS total_value,
    CAST(ROUND(ABS(t.value)) AS INTEGER) AS abs_total,
    CASE WHEN t.value < 0 THEN -1 ELSE 1 END AS sign
  FROM stat_occurrences t
  JOIN patterns p ON p.total_abs = CAST(ROUND(ABS(t.value)) AS INTEGER)
  WHERE t.context_type = 'Specialty'
    AND t.is_total = 1
    AND t.level = 5
    AND UPPER(t.context_name) LIKE 'SPECIALTY %'
    AND CAST(TRIM(SUBSTR(t.context_name, 10)) AS INTEGER) > 0
),
eligible AS (
  SELECT *
  FROM targets tgt
  WHERE NOT EXISTS (
    SELECT 1
    FROM stat_occurrences x
    WHERE x.general_id = tgt.general_id
      AND x.stat_key_id = tgt.stat_key_id
      AND x.context_type = 'Specialty'
      AND x.is_total = 0
      AND x.level BETWEEN 1 AND 4
      AND CAST(TRIM(SUBSTR(x.context_name, 10)) AS INTEGER) = tgt.spec_n
  )
)
SELECT
  e.general_id,
  e.stat_key_id,
  e.spec_n,
  e.total_id,
  e.sign,
  p.l1,
  p.l2,
  p.l3,
  p.l4,
  p.l5,
  e.abs_total
FROM eligible e
JOIN patterns p ON p.total_abs = e.abs_total
ORDER BY e.general_id, e.spec_n, e.stat_key_id;")

tmp_sql="$(mktemp)"
trap 'rm -f "$tmp_sql"' EXIT

{
  echo "BEGIN TRANSACTION;"
} > "$tmp_sql"

group_count=0
insert_stmt_count=0
while IFS='|' read -r general_id stat_key_id spec_n total_id sign l1 l2 l3 l4 l5 abs_total; do
  [[ -z "${general_id:-}" ]] && continue
  group_count=$((group_count + 1))

  vals=("$l1" "$l2" "$l3" "$l4" "$l5")
  for i in 1 2 3 4 5; do
    idx=$((i - 1))
    inc=${vals[$idx]}
    value=$((sign * inc))
    cat >> "$tmp_sql" <<SQL
INSERT INTO stat_occurrences (
  general_id, stat_key_id, value, context_type, context_name,
  level, is_total, file_path, line_number, raw_line,
  origin, generated_from_total_id, edited_by_user, stat_checked_in_game
)
SELECT
  $general_id, $stat_key_id, $value, 'Specialty', 'SPECIALTY $spec_n L$i',
  $i, 0, '', 0, 'MIGRATE_SPECIALTY_LEVEL_FROM_TOTAL $abs_total',
  'generated', $total_id, 0, 0
WHERE NOT EXISTS (
  SELECT 1 FROM stat_occurrences x
  WHERE x.general_id = $general_id
    AND x.stat_key_id = $stat_key_id
    AND x.context_type = 'Specialty'
    AND x.is_total = 0
    AND x.level = $i
    AND CAST(TRIM(SUBSTR(x.context_name, 10)) AS INTEGER) = $spec_n
);
SQL
    insert_stmt_count=$((insert_stmt_count + 1))
  done
done <<< "$rows_tsv"

echo "COMMIT;" >> "$tmp_sql"

sqlite3 "$DB_PATH" < "$tmp_sql"

echo "Eligible specialty groups: $group_count"
echo "Insert statements evaluated: $insert_stmt_count"

sqlite3 "$DB_PATH" <<'SQL'
.headers on
.mode column
SELECT 'singleton_l5_normalized' AS metric,
       COUNT(*) AS value
FROM stat_occurrences
WHERE raw_line = 'MIGRATE_SINGLETON_L5_TO_INCREMENT';

SELECT 'level_rows_backfilled' AS metric,
       COUNT(*) AS value
FROM stat_occurrences
WHERE raw_line LIKE 'MIGRATE_SPECIALTY_LEVEL_FROM_TOTAL %';
SQL

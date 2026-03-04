PRAGMA foreign_keys = ON;

-- ==========================================
-- GENERALS
-- ==========================================
CREATE TABLE generals (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL UNIQUE,
    role            TEXT NOT NULL,
    role_confirmed  INTEGER NOT NULL DEFAULT 1,
    in_tavern       INTEGER NOT NULL CHECK (in_tavern IN (0,1)),
    base_skill_name TEXT NOT NULL,

    leadership      INTEGER NOT NULL,
    leadership_green REAL NOT NULL,

    attack          INTEGER NOT NULL,
    attack_green    REAL NOT NULL,

    defense         INTEGER NOT NULL,
    defense_green   REAL NOT NULL,

    politics        INTEGER NOT NULL,
    politics_green  REAL NOT NULL
);

CREATE TRIGGER trg_generals_role_valid_insert
BEFORE INSERT ON generals
FOR EACH ROW
BEGIN
  SELECT CASE
    WHEN NEW.role NOT IN ('Ground','Mounted','Ranged','Siege','Defense','Mixed','Admin','Duty','Mayor','Unknown')
    THEN RAISE(ABORT, 'Invalid generals.role')
  END;
END;

CREATE TRIGGER trg_generals_role_valid_update
BEFORE UPDATE OF role ON generals
FOR EACH ROW
BEGIN
  SELECT CASE
    WHEN NEW.role NOT IN ('Ground','Mounted','Ranged','Siege','Defense','Mixed','Admin','Duty','Mayor','Unknown')
    THEN RAISE(ABORT, 'Invalid generals.role')
  END;
END;

-- ==========================================
-- CANONICAL STAT KEYS
-- ==========================================
CREATE TABLE stat_keys (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    key         TEXT NOT NULL UNIQUE,
    kind        TEXT NOT NULL CHECK (kind IN ('percent','flat','unknown')),
    is_active   INTEGER NOT NULL DEFAULT 1,
    notes       TEXT
);

-- ==========================================
-- ALIASES
-- ==========================================
CREATE TABLE stat_key_aliases (
    alias_key     TEXT PRIMARY KEY,
    stat_key_id   INTEGER NOT NULL,
    FOREIGN KEY (stat_key_id) REFERENCES stat_keys(id)
);

-- ==========================================
-- ALIAS TRANSFORMS (FOR MERGES / NORMALIZATION)
-- ==========================================
CREATE TABLE stat_key_transforms (
    alias_key     TEXT PRIMARY KEY,      -- same string as stat_key_aliases.alias_key
    stat_key_id   INTEGER NOT NULL,       -- should match stat_key_aliases.stat_key_id
    multiplier    REAL NOT NULL DEFAULT 1.0,
    notes         TEXT,

    FOREIGN KEY (alias_key) REFERENCES stat_key_aliases(alias_key) ON DELETE CASCADE,
    FOREIGN KEY (stat_key_id) REFERENCES stat_keys(id) ON DELETE CASCADE
);

CREATE INDEX idx_transforms_statkey ON stat_key_transforms(stat_key_id);


-- ==========================================
-- STAT OCCURRENCES (NO DATA LOSS)
-- ==========================================
CREATE TABLE stat_occurrences (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    general_id      INTEGER NOT NULL,
    stat_key_id     INTEGER NOT NULL,

    value           REAL NOT NULL,

    context_type    TEXT NOT NULL CHECK (context_type IN ('BaseSkill','Ascension','Specialty','Covenant')),
    context_name    TEXT NOT NULL,

    level           INTEGER,
    is_total        INTEGER NOT NULL DEFAULT 0,

    file_path       TEXT NOT NULL,
    line_number     INTEGER NOT NULL,
    raw_line        TEXT NOT NULL,
    origin          TEXT NOT NULL DEFAULT 'imported' CHECK (origin IN ('imported','generated','gui')),
    generated_from_total_id INTEGER,
    edited_by_user  INTEGER NOT NULL DEFAULT 0 CHECK (edited_by_user IN (0,1)),
    stat_checked_in_game INTEGER NOT NULL DEFAULT 0,

    FOREIGN KEY (general_id) REFERENCES generals(id),
    FOREIGN KEY (stat_key_id) REFERENCES stat_keys(id)
);

-- ==========================================
-- PENDING UNKNOWN STAT KEYS
-- ==========================================
CREATE TABLE pending_stat_keys (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    raw_key         TEXT NOT NULL UNIQUE,
    status          TEXT NOT NULL CHECK (status IN ('pending','approved','rejected','mapped')),
    first_seen_file TEXT NOT NULL,
    first_seen_line INTEGER NOT NULL
);

-- ==========================================
-- PENDING EXAMPLES
-- ==========================================
CREATE TABLE pending_stat_key_examples (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    pending_id      INTEGER NOT NULL,
    general_name    TEXT NOT NULL,
    context_type    TEXT NOT NULL,
    context_name    TEXT NOT NULL,
    level           INTEGER,
    value           REAL NOT NULL,
    file_path       TEXT NOT NULL,
    line_number     INTEGER NOT NULL,
    raw_line        TEXT NOT NULL,

    FOREIGN KEY (pending_id) REFERENCES pending_stat_keys(id)
);

-- ==========================================
-- INDEXES
-- ==========================================
CREATE INDEX idx_stat_occ_general ON stat_occurrences(general_id);
CREATE INDEX idx_stat_occ_statkey ON stat_occurrences(stat_key_id);
CREATE INDEX idx_alias_statkey ON stat_key_aliases(stat_key_id);
CREATE INDEX idx_pending_status ON pending_stat_keys(status);

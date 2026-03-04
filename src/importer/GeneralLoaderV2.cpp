// src/importer/GeneralLoaderV2.cpp
#include "GeneralLoaderV2.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

// ----------------------
// trim helpers
// ----------------------
static inline std::string trim(std::string s) {
  auto notspace = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
  s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
  return s;
}

static inline bool starts_with_ci(std::string_view s, std::string_view prefix) {
  if (s.size() < prefix.size())
    return false;
  for (size_t i = 0; i < prefix.size(); ++i) {
    unsigned char a = static_cast<unsigned char>(s[i]);
    unsigned char b = static_cast<unsigned char>(prefix[i]);
    if (std::tolower(a) != std::tolower(b))
      return false;
  }
  return true;
}

static inline bool contains_ci(std::string_view s, std::string_view needle) {
  if (needle.empty())
    return true;
  if (s.size() < needle.size())
    return false;

  for (size_t i = 0; i + needle.size() <= s.size(); ++i) {
    bool ok = true;
    for (size_t j = 0; j < needle.size(); ++j) {
      unsigned char a = static_cast<unsigned char>(s[i + j]);
      unsigned char b = static_cast<unsigned char>(needle[j]);
      if (std::tolower(a) != std::tolower(b)) {
        ok = false;
        break;
      }
    }
    if (ok)
      return true;
  }
  return false;
}

static inline void replace_all(std::string &s, std::string_view from,
                               std::string_view to) {
  if (from.empty())
    return;
  size_t pos = 0;
  while ((pos = s.find(from.data(), pos, from.size())) != std::string::npos) {
    s.replace(pos, from.size(), to.data(), to.size());
    pos += to.size();
  }
}

static inline std::vector<std::string> split_ws(const std::string &s) {
  std::istringstream iss(s);
  std::vector<std::string> out;
  std::string tok;
  while (iss >> tok)
    out.push_back(tok);
  return out;
}

// ----------------------
// comment / separator detection
// ----------------------
static inline bool is_separator_line(std::string_view t) {
  bool any = false;
  for (char c : t) {
    if (std::isspace(static_cast<unsigned char>(c)))
      continue;
    any = true;
    if (c != '-' && c != '=')
      return false;
  }
  return any;
}

static inline bool is_comment_or_blank_or_separator(const std::string &line) {
  std::string t = trim(line);
  if (t.empty())
    return true;
  if (t.front() == '#')
    return true;
  if (is_separator_line(t))
    return true;
  return false;
}

// ----------------------
// banned raw key rules
// ----------------------
static inline bool is_banned_raw_key(std::string_view key) {
  if (key.empty())
    return true;
  if (key == "#" || key == "SPECIALTY" || key == "SPECIALTIES")
    return true;
  if (!key.empty() && key.front() == '#')
    return true;

  // Only allow [A-Za-z0-9_]
  bool has_alnum = false;
  for (char c : key) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc) || c == '_') {
      if (std::isalnum(uc))
        has_alnum = true;
      continue;
    }
    return true; // punctuation -> not a key token
  }
  return !has_alnum;
}

// ----------------------
// number parsing
// ----------------------
static inline bool looks_like_number(std::string_view s) {
  if (s.empty())
    return false;
  if (s.front() == '+' || s.front() == '-')
    s.remove_prefix(1);
  if (s.empty())
    return false;

  bool seen_digit = false;
  bool seen_dot = false;
  for (char ch : s) {
    unsigned char uc = static_cast<unsigned char>(ch);
    if (std::isdigit(uc)) {
      seen_digit = true;
      continue;
    }
    if (ch == '.' && !seen_dot) {
      seen_dot = true;
      continue;
    }
    return false;
  }
  return seen_digit;
}

static inline std::optional<double> parse_number(std::string_view s) {
  std::string tmp(s);
  char *end = nullptr;
  errno = 0;
  double v = std::strtod(tmp.c_str(), &end);
  if (errno != 0 || end == tmp.c_str())
    return std::nullopt;
  while (*end != '\0') {
    if (!std::isspace(static_cast<unsigned char>(*end)))
      return std::nullopt;
    ++end;
  }
  return v;
}

static inline std::optional<int> parse_int(std::string_view s) {
  std::string tmp(s);
  char *end = nullptr;
  errno = 0;
  long v = std::strtol(tmp.c_str(), &end, 10);
  if (errno != 0 || end == tmp.c_str())
    return std::nullopt;
  while (*end != '\0') {
    if (!std::isspace(static_cast<unsigned char>(*end)))
      return std::nullopt;
    ++end;
  }
  if (v < std::numeric_limits<int>::min() ||
      v > std::numeric_limits<int>::max())
    return std::nullopt;
  return static_cast<int>(v);
}

static inline bool parse_bool_ci(std::string_view v, bool &out) {
  if (starts_with_ci(v, "true") || starts_with_ci(v, "yes") ||
      starts_with_ci(v, "1")) {
    out = true;
    return true;
  }
  if (starts_with_ci(v, "false") || starts_with_ci(v, "no") ||
      starts_with_ci(v, "0")) {
    out = false;
    return true;
  }
  return false;
}

// ----------------------
// kv parsing: "KEY: value" or "KEY=value"
// ----------------------
static inline bool parse_kv_line(const std::string &t, std::string &key_out,
                                 std::string &val_out) {
  auto pos_colon = t.find(':');
  auto pos_eq = t.find('=');

  size_t pos = std::string::npos;
  if (pos_colon != std::string::npos)
    pos = pos_colon;
  if (pos_eq != std::string::npos) {
    if (pos == std::string::npos || pos_eq < pos)
      pos = pos_eq;
  }
  if (pos == std::string::npos)
    return false;

  key_out = trim(t.substr(0, pos));
  val_out = trim(t.substr(pos + 1));
  return !key_out.empty();
}

// ----------------------
// v3.x core-attribute parsing: "131 (8.89)"
// ----------------------
static inline std::optional<double> parse_paren_number(std::string_view s) {
  std::string tmp = trim(std::string(s));
  auto l = tmp.find('(');
  auto r = tmp.find(')', (l == std::string::npos) ? 0 : (l + 1));
  if (l == std::string::npos || r == std::string::npos || r <= l + 1)
    return std::nullopt;
  return parse_number(trim(tmp.substr(l + 1, r - (l + 1))));
}

static inline bool parse_int_with_optional_green(const std::string &v_raw,
                                                 int &out_int,
                                                 double &out_green) {
  std::string v = trim(v_raw);

  if (auto g = parse_paren_number(v))
    out_green = *g;

  auto l = v.find('(');
  std::string int_part = trim(l == std::string::npos ? v : v.substr(0, l));
  if (auto iv = parse_int(int_part)) {
    out_int = *iv;
    return true;
  }
  return false;
}

// ----------------------
// context + specialty parsing
// ----------------------
struct Context {
  std::string type; // BaseSkill / Ascension / Specialty / Covenant
  std::string name; // Base skill name, ascension header, covenant header, etc
  std::optional<int> level;        // Specialty only
  bool specialty_is_total = false; // Specialty only (explicit TOTAL headers)
};

static inline void set_context(Context &ctx, std::string type, std::string name,
                               std::optional<int> level = std::nullopt,
                               bool specialty_is_total = false) {
  ctx.type = std::move(type);
  ctx.name = std::move(name);
  ctx.level = level;
  ctx.specialty_is_total = specialty_is_total;
}

static int covenant_number_from_label_ci(std::string_view label) {
  // Evony covenant order is fixed in-game:
  // 1 War, 2 Cooperation, 3 Peace, 4 Faith, 5 Honor, 6 Civilization
  auto lower = [](unsigned char c) { return (char)std::tolower(c); };

  std::string s;
  s.reserve(label.size());
  for (char c : label)
    s.push_back(lower((unsigned char)c));
  s = trim(std::move(s));

  if (s == "war")
    return 1;
  if (s == "cooperation")
    return 2;
  if (s == "peace")
    return 3;
  if (s == "faith")
    return 4;
  if (s == "honor")
    return 5;
  if (s == "civilization")
    return 6;
  return 0;
}

static int first_int_1_to_6(std::string_view text) {
  int v = 0;
  bool seen_digit = false;
  for (char c : text) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (!std::isdigit(uc)) {
      if (seen_digit)
        break;
      continue;
    }
    seen_digit = true;
    v = (v * 10) + (c - '0');
    if (v > 6)
      return 0;
  }
  return (v >= 1 && v <= 6) ? v : 0;
}

static int covenant_number_from_comment_line_ci(std::string_view raw_line) {
  // Accept common variants:
  // "# War Covenant", "# Covenant 1", "# Covenant 1 | War"
  std::string s = trim(std::string(raw_line));
  if (s.empty())
    return 0;
  if (!s.empty() && s.front() == '#')
    s = trim(s.substr(1));
  if (!s.empty() && s.back() == ':')
    s.pop_back();
  s = trim(std::move(s));
  if (s.empty())
    return 0;

  if (int n = first_int_1_to_6(s); n > 0)
    return n;

  // Remove trailing " covenant" and map the remaining label.
  std::string lower = s;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  std::string suffix = " covenant";
  if (lower.size() > suffix.size() &&
      lower.rfind(suffix) == (lower.size() - suffix.size())) {
    return covenant_number_from_label_ci(trim(lower.substr(
        0, static_cast<size_t>(lower.size() - suffix.size()))));
  }

  return covenant_number_from_label_ci(lower);
}

static inline std::optional<int>
parse_specialty_level_from_header(const std::string &t) {
  // Look for 'L1'..'L5' anywhere.
  for (size_t i = 0; i + 1 < t.size(); ++i) {
    if ((t[i] == 'L' || t[i] == 'l') &&
        std::isdigit(static_cast<unsigned char>(t[i + 1]))) {
      int lvl = t[i + 1] - '0';
      if (lvl >= 1 && lvl <= 5)
        return lvl;
    }
  }
  return std::nullopt;
}

static inline std::string normalize_specialty_context_name(std::string t) {
  // "SPECIALTY 1 L3:" -> "SPECIALTY 1"
  t = trim(std::move(t));
  if (!t.empty() && t.back() == ':')
    t.pop_back();
  t = trim(std::move(t));

  // Find " Lx" and truncate
  for (size_t i = 0; i + 2 < t.size(); ++i) {
    if (t[i] == ' ' && (t[i + 1] == 'L' || t[i + 1] == 'l') &&
        std::isdigit(static_cast<unsigned char>(t[i + 2]))) {
      return trim(t.substr(0, i));
    }
  }
  return t;
}

static inline bool parse_header_context(const std::string &t_raw, Context &ctx,
                                        GeneralMetaV2 &meta) {
  std::string t = trim(t_raw);
  if (t.empty())
    return false;

  // Specialty
  if (starts_with_ci(t, "SPECIALTIES")) {
    set_context(ctx, "Specialty", "SPECIALTIES", std::nullopt);
    return true;
  }
  if (starts_with_ci(t, "SPECIALTY")) {
    auto lvl = parse_specialty_level_from_header(t);
    std::string name = normalize_specialty_context_name(t);

    // Only treat as TOTAL if the header explicitly says so, e.g.
    // "SPECIALTY 1 L5 (TOTAL):"
    const bool is_total = contains_ci(t, "TOTAL");

    set_context(ctx, "Specialty", name, lvl, is_total);
    return true;
  }

  // v3.x: "BASE SKILL | <name>"
  if (starts_with_ci(t, "BASE SKILL")) {
    std::string base_name = "BaseSkill";
    auto bar = t.find('|');
    if (bar != std::string::npos) {
      base_name = trim(t.substr(bar + 1));
      if (!base_name.empty())
        meta.base_skill_name = base_name;
    }
    set_context(ctx, "BaseSkill", base_name.empty() ? "BaseSkill" : base_name);
    return true;
  }

  // v2/v3: "BaseSkill" or "Base Skill"
  if (starts_with_ci(t, "BaseSkill") || starts_with_ci(t, "Base Skill")) {
    set_context(ctx, "BaseSkill",
                meta.base_skill_name.empty() ? "BaseSkill"
                                             : meta.base_skill_name);
    return true;
  }

  // Ascension headers ("ASCENSION 1", etc.)
  if (starts_with_ci(t, "ASCENSION")) {
    set_context(ctx, "Ascension", t);
    return true;
  }

  // Covenant headers ("COVENANT | War", etc.)
  if (starts_with_ci(t, "COVENANTS")) {
    // Section header; concrete covenant index usually appears in following
    // comment headers such as "# War Covenant".
    set_context(ctx, "Covenant", "COVENANTS");
    return true;
  }

  if (starts_with_ci(t, "COVENANT")) {
    int n = 0;

    auto bar = t.find('|');
    if (bar != std::string::npos) {
      std::string label = trim(t.substr(bar + 1));
      n = covenant_number_from_label_ci(label);
    }
    if (n <= 0)
      n = first_int_1_to_6(t);

    if (n > 0) {
      set_context(ctx, "Covenant", "COVENANT " + std::to_string(n));
    } else {
      // Fallback: keep whatever was in the file
      set_context(ctx, "Covenant", t);
    }
    return true;
  }

  return false;
}

// ----------------------
// stat line parsing
// ----------------------
struct ParsedStat {
  std::string key;
  double value = 0.0;
};

static inline std::string strip_trailing_paren_comment(std::string s) {
  auto pos = s.find(" (");
  if (pos != std::string::npos)
    s = s.substr(0, pos);
  return trim(std::move(s));
}

static inline std::optional<ParsedStat>
parse_stat_line(const std::string &raw_line) {
  std::string t = raw_line;
  replace_all(t, "\t", " ");
  t = trim(std::move(t));

  if (t.empty())
    return std::nullopt;
  if (is_comment_or_blank_or_separator(t))
    return std::nullopt;

  // Do NOT skip SPECIALTY headers here; header parsing runs before stat
  // parsing.

  t = strip_trailing_paren_comment(std::move(t));

  auto toks = split_ws(t);
  if (toks.size() < 2)
    return std::nullopt;

  const std::string &key = toks[0];
  if (is_banned_raw_key(key))
    return std::nullopt;

  if (!looks_like_number(toks[1]))
    return std::nullopt;

  auto v = parse_number(toks[1]);
  if (!v.has_value())
    return std::nullopt;

  return ParsedStat{key, *v};
}

} // namespace

LoadedGeneralV2 load_general_v2_from_file(const std::string &path) {
  LoadedGeneralV2 out;
  out.meta = GeneralMetaV2{};
  out.meta.in_tavern = false;
  out.meta.double_checked_in_game = false;

  std::ifstream in(path);
  if (!in) {
    out.errors.push_back("Failed to open file: " + path);
    return out;
  }

  Context ctx;
  ctx.type.clear();
  ctx.name.clear();
  ctx.level = std::nullopt;

  bool in_source_verbatim = false;

  std::string line;
  int line_no = 0;

  while (std::getline(in, line)) {
    ++line_no;

    // ------------------------------------------------------------
    // SOURCE TEXT capture MUST happen BEFORE comment/separator skip
    // Your files use "# SOURCE TEXT (VERBATIM)" and often no END marker.
    // ------------------------------------------------------------
    {
      std::string t2 = trim(line);

      // Start marker (allow leading '#')
      if (!in_source_verbatim && (starts_with_ci(t2, "SOURCE TEXT") ||
                                  starts_with_ci(t2, "# SOURCE TEXT"))) {
        in_source_verbatim = true;
        out.meta.source_text_verbatim += line;
        out.meta.source_text_verbatim += "\n";
        continue;
      }

      // Optional end marker (if present in any files)
      if (in_source_verbatim && (starts_with_ci(t2, "END SOURCE TEXT") ||
                                 starts_with_ci(t2, "# END SOURCE TEXT"))) {
        in_source_verbatim = false;
        out.meta.source_text_verbatim += line;
        out.meta.source_text_verbatim += "\n";
        continue;
      }

      // If we're inside verbatim, capture everything exactly as-is.
      if (in_source_verbatim) {
        out.meta.source_text_verbatim += line;
        out.meta.source_text_verbatim += "\n";
        continue;
      }
    }

    // Normal parsing line (trimmed)
    std::string t = trim(line);

    // Legacy covenant files often encode covenant index in comments:
    // "# War Covenant", "# Covenant 1", etc.
    if (!t.empty() && t.front() == '#' && ctx.type == "Covenant") {
      int n = covenant_number_from_comment_line_ci(t);
      if (n > 0) {
        set_context(ctx, "Covenant", "COVENANT " + std::to_string(n));
        continue;
      }
    }

    if (is_comment_or_blank_or_separator(t))
      continue;

    // Meta key/value
    {
      std::string k, v;
      if (parse_kv_line(t, k, v)) {
        std::string k_up = k;
        std::transform(
            k_up.begin(), k_up.end(), k_up.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

        if (k_up == "NAME" || k_up == "GENERAL" || k_up == "GENERAL_NAME" ||
            k_up == "GENERAL NAME") {
          out.meta.name = v;
          continue;
        }

        if (k_up == "ROLE") {
          out.meta.role = v;
          continue;
        }

        if (k_up == "IN_TAVERN" || k_up == "INTAVERN") {
          bool b = false;
          if (parse_bool_ci(v, b))
            out.meta.in_tavern = b;
          else
            out.errors.push_back(path + ":" + std::to_string(line_no) +
                                 " invalid IN_TAVERN/InTavern: " + v);
          continue;
        }

        if (k_up == "BASE_SKILL_NAME" || k_up == "BASE SKILL NAME") {
          out.meta.base_skill_name = v;
          continue;
        }

        if (k_up == "DOUBLE_CHECKED_IN_GAME" ||
            k_up == "DOUBLE CHECKED IN GAME") {
          bool b = false;
          if (parse_bool_ci(v, b))
            out.meta.double_checked_in_game = b;
          else
            out.errors.push_back(path + ":" + std::to_string(line_no) +
                                 " invalid DOUBLE_CHECKED_IN_GAME: " + v);
          continue;
        }

        // Core attributes (v2: "103" ; v3: "103 (8.68)")
        if (k_up == "LEADERSHIP") {
          if (!parse_int_with_optional_green(v, out.meta.leadership,
                                             out.meta.leadership_green))
            out.errors.push_back(path + ":" + std::to_string(line_no) +
                                 " invalid Leadership: " + v);
          continue;
        }

        if (k_up == "ATTACK") {
          if (!parse_int_with_optional_green(v, out.meta.attack,
                                             out.meta.attack_green))
            out.errors.push_back(path + ":" + std::to_string(line_no) +
                                 " invalid Attack: " + v);
          continue;
        }

        if (k_up == "DEFENSE") {
          if (!parse_int_with_optional_green(v, out.meta.defense,
                                             out.meta.defense_green))
            out.errors.push_back(path + ":" + std::to_string(line_no) +
                                 " invalid Defense: " + v);
          continue;
        }

        if (k_up == "POLITICS") {
          if (!parse_int_with_optional_green(v, out.meta.politics,
                                             out.meta.politics_green))
            out.errors.push_back(path + ":" + std::to_string(line_no) +
                                 " invalid Politics: " + v);
          continue;
        }

        // Optional explicit green lines
        if (k_up == "LEADERSHIP_GREEN" || k_up == "LEADERSHIPGREEN") {
          if (auto dv = parse_number(v))
            out.meta.leadership_green = *dv;
          else
            out.errors.push_back(path + ":" + std::to_string(line_no) +
                                 " invalid LeadershipGreen: " + v);
          continue;
        }
        if (k_up == "ATTACK_GREEN" || k_up == "ATTACKGREEN") {
          if (auto dv = parse_number(v))
            out.meta.attack_green = *dv;
          else
            out.errors.push_back(path + ":" + std::to_string(line_no) +
                                 " invalid AttackGreen: " + v);
          continue;
        }
        if (k_up == "DEFENSE_GREEN" || k_up == "DEFENSEGREEN") {
          if (auto dv = parse_number(v))
            out.meta.defense_green = *dv;
          else
            out.errors.push_back(path + ":" + std::to_string(line_no) +
                                 " invalid DefenseGreen: " + v);
          continue;
        }
        if (k_up == "POLITICS_GREEN" || k_up == "POLITICSGREEN") {
          if (auto dv = parse_number(v))
            out.meta.politics_green = *dv;
          else
            out.errors.push_back(path + ":" + std::to_string(line_no) +
                                 " invalid PoliticsGreen: " + v);
          continue;
        }
      }
    }

    // Section/context headers
    if (parse_header_context(t, ctx, out.meta))
      continue;

    // Stat line
    if (auto st = parse_stat_line(t)) {
      StatOccurrenceV2 occ;
      occ.raw_key = std::move(st->key);
      occ.value = st->value;

      occ.context_type = ctx.type.empty() ? "Unknown" : ctx.type;
      occ.context_name = ctx.name;

      if (occ.context_type == "Specialty") {
        occ.level = ctx.level;

        // IMPORTANT:
        // - In split files, "SPECIALTY 1 L5:" is NOT a TOTAL; it's the L5
        // increment.
        // - Only headers that explicitly contain "TOTAL" should set
        // is_total=true.
        occ.is_total = ctx.specialty_is_total;

        // If it is a TOTAL header but no level was parsed, force it to L5.
        if (occ.is_total) {
          if (!occ.level.has_value() || *occ.level != 5)
            occ.level = 5;
        }
      } else {

        occ.level = std::nullopt;
        occ.is_total = false;
      }

      occ.file_path = path;
      occ.line_number = line_no;
      occ.raw_line =
          t; // trimmed, but original text is in source_text_verbatim if present

      if (!is_banned_raw_key(occ.raw_key))
        out.occurrences.push_back(std::move(occ));
      continue;
    }
  }

  // Fallback name from filename, normalized (Ahmose_I -> Ahmose I)
  if (out.meta.name.empty()) {
    std::string name = path;
    auto slash = name.find_last_of("/\\");
    if (slash != std::string::npos)
      name = name.substr(slash + 1);
    auto dot = name.rfind('.');
    if (dot != std::string::npos)
      name = name.substr(0, dot);
    for (char &c : name) {
      if (c == '_')
        c = ' ';
    }
    out.meta.name = name;
  }

  return out;
}

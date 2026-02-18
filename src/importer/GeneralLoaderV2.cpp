#include "GeneralLoaderV2.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>

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
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        unsigned char a = static_cast<unsigned char>(s[i]);
        unsigned char b = static_cast<unsigned char>(prefix[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
}

static inline bool starts_with(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

static inline void replace_all(std::string& s, std::string_view from, std::string_view to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from.data(), pos, from.size())) != std::string::npos) {
        s.replace(pos, from.size(), to.data(), to.size());
        pos += to.size();
    }
}

static inline std::vector<std::string> split_ws(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

// ----------------------
// line filters (THE WALL)
// ----------------------
static inline bool is_separator_line(std::string_view t) {
    bool any = false;
    for (char c : t) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        any = true;
        if (c != '-' && c != '=' ) return false;
    }
    return any; // all non-space were - or =
}

static inline bool is_comment_or_blank_or_separator(const std::string& line) {
    std::string t = trim(line);
    if (t.empty()) return true;
    if (t.front() == '#') return true;
    if (is_separator_line(t)) return true;
    return false;
}

static inline bool is_specialty_marker_line(const std::string& line) {
    // This catches:
    // "SPECIALTY"
    // "SPECIALTY 1 L5:"
    // "SPECIALTY 2 L1:"
    // and any capitalization variants.
    std::string t = trim(line);
    if (t.empty()) return false;
    return starts_with_ci(t, "SPECIALTY");
}

static inline bool is_banned_raw_key(std::string_view key) {
    if (key.empty()) return true;

    // Your two offenders (hard block)
    if (key == "#" || key == "SPECIALTY") return true;

    // Anything starting with '#'
    if (!key.empty() && key.front() == '#') return true;

    // Only allow [A-Za-z0-9_]
    bool has_alnum = false;
    for (char c : key) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '_') {
            if (std::isalnum(uc)) has_alnum = true;
            continue;
        }
        return true; // punctuation -> not a valid key token
    }
    return !has_alnum;
}

// ----------------------
// number parsing
// ----------------------
static inline bool looks_like_number(std::string_view s) {
    if (s.empty()) return false;
    if (s.front() == '+' || s.front() == '-') s.remove_prefix(1);
    if (s.empty()) return false;

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
    char* end = nullptr;
    errno = 0;
    double v = std::strtod(tmp.c_str(), &end);
    if (errno != 0 || end == tmp.c_str()) return std::nullopt;
    while (*end != '\0') {
        if (!std::isspace(static_cast<unsigned char>(*end))) return std::nullopt;
        ++end;
    }
    return v;
}

static inline std::optional<int> parse_int(std::string_view s) {
    std::string tmp(s);
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(tmp.c_str(), &end, 10);
    if (errno != 0 || end == tmp.c_str()) return std::nullopt;
    while (*end != '\0') {
        if (!std::isspace(static_cast<unsigned char>(*end))) return std::nullopt;
        ++end;
    }
    if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) return std::nullopt;
    return static_cast<int>(v);
}

static inline bool parse_bool_ci(std::string_view v, bool& out) {
    if (starts_with_ci(v, "true") || starts_with_ci(v, "yes") || starts_with_ci(v, "1")) { out = true; return true; }
    if (starts_with_ci(v, "false") || starts_with_ci(v, "no")  || starts_with_ci(v, "0")) { out = false; return true; }
    return false;
}

// ----------------------
// meta parsing
// ----------------------
static inline bool parse_kv_line(const std::string& t, std::string& key_out, std::string& val_out) {
    // Accept "Key: Value" or "Key = Value"
    auto pos_colon = t.find(':');
    auto pos_eq    = t.find('=');

    size_t pos = std::string::npos;
    if (pos_colon != std::string::npos) pos = pos_colon;
    if (pos_eq != std::string::npos) {
        if (pos == std::string::npos || pos_eq < pos) pos = pos_eq;
    }
    if (pos == std::string::npos) return false;

    key_out = trim(t.substr(0, pos));
    val_out = trim(t.substr(pos + 1));
    return !key_out.empty();
}

// ----------------------
// context
// ----------------------
struct Context {
    std::string type; // BaseSkill / Ascension / Specialty / Covenant
    std::string name;
};

static inline void set_context(Context& ctx, std::string type, std::string name) {
    ctx.type = std::move(type);
    ctx.name = std::move(name);
}

static inline bool parse_header_context(const std::string& t_raw, Context& ctx) {
    std::string t = trim(t_raw);

    // IMPORTANT: do NOT treat "SPECIALTY ..." marker lines as context headers here,
    // because those have caused junk parsing elsewhere. We'll skip them earlier.
    if (starts_with_ci(t, "BaseSkill") || starts_with_ci(t, "Base Skill")) {
        set_context(ctx, "BaseSkill", "BaseSkill");
        return true;
    }
    if (starts_with_ci(t, "Ascension")) {
        set_context(ctx, "Ascension", t);
        return true;
    }
    if (starts_with_ci(t, "Specialty")) {
        set_context(ctx, "Specialty", t);
        return true;
    }
    if (starts_with_ci(t, "Covenant")) {
        set_context(ctx, "Covenant", t);
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
    // Remove " ( ... )" and everything after, if present
    auto pos = s.find(" (");
    if (pos != std::string::npos) s = s.substr(0, pos);
    return trim(std::move(s));
}

static inline std::optional<ParsedStat> parse_stat_line(const std::string& raw_line) {
    std::string t = raw_line;
    replace_all(t, "\t", " ");
    t = trim(std::move(t));

    if (t.empty()) return std::nullopt;
    if (is_comment_or_blank_or_separator(t)) return std::nullopt;
    if (is_specialty_marker_line(t)) return std::nullopt; // THE WALL
    // Kill trailing "(when ...)" comments
    t = strip_trailing_paren_comment(std::move(t));

    auto toks = split_ws(t);
    if (toks.size() < 2) return std::nullopt;

    const std::string& key = toks[0];
    if (is_banned_raw_key(key)) return std::nullopt;

    if (!looks_like_number(toks[1])) return std::nullopt;
    auto v = parse_number(toks[1]);
    if (!v.has_value()) return std::nullopt;

    return ParsedStat{key, *v};
}

static inline std::optional<int> parse_specialty_level_from_context_name(const std::string&) {
    // Leave nullopt; your specialty expansion logic lives elsewhere.
    return std::nullopt;
}

} // namespace

LoadedGeneralV2 load_general_v2_from_file(const std::string& path) {
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

    bool in_source_verbatim = false;

    std::string line;
    int line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;
        std::string t = trim(line);

        // Hard skip: comment / blank / separator
        if (is_comment_or_blank_or_separator(t)) continue;

        // Hard skip: "SPECIALTY ..." marker lines (prevents re-adding forever)
        if (is_specialty_marker_line(t)) continue;

        // SOURCE TEXT capture
        if (starts_with_ci(t, "SOURCE TEXT")) {
            in_source_verbatim = true;
            out.meta.source_text_verbatim += line;
            out.meta.source_text_verbatim += "\n";
            continue;
        }
        if (starts_with_ci(t, "END SOURCE TEXT")) {
            in_source_verbatim = false;
            out.meta.source_text_verbatim += line;
            out.meta.source_text_verbatim += "\n";
            continue;
        }
        if (in_source_verbatim) {
            out.meta.source_text_verbatim += line;
            out.meta.source_text_verbatim += "\n";
            continue;
        }

        // Meta key/value
        {
            std::string k, v;
            if (parse_kv_line(t, k, v)) {
                std::string k_up = k;
                std::transform(k_up.begin(), k_up.end(), k_up.begin(),
                               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

                if (k_up == "NAME") { out.meta.name = v; continue; }
                if (k_up == "ROLE") { out.meta.role = v; continue; }

                if (k_up == "IN_TAVERN") {
                    bool b = false;
                    if (parse_bool_ci(v, b)) out.meta.in_tavern = b;
                    else out.errors.push_back(path + ":" + std::to_string(line_no) + " invalid IN_TAVERN: " + v);
                    continue;
                }

                if (k_up == "BASE_SKILL_NAME" || k_up == "BASE SKILL NAME") {
                    out.meta.base_skill_name = v;
                    continue;
                }

                if (k_up == "DOUBLE_CHECKED_IN_GAME" || k_up == "DOUBLE CHECKED IN GAME") {
                    bool b = false;
                    if (parse_bool_ci(v, b)) out.meta.double_checked_in_game = b;
                    else out.errors.push_back(path + ":" + std::to_string(line_no) + " invalid DOUBLE_CHECKED_IN_GAME: " + v);
                    continue;
                }

                if (k_up == "LEADERSHIP") {
                    if (auto iv = parse_int(v)) out.meta.leadership = *iv;
                    else out.errors.push_back(path + ":" + std::to_string(line_no) + " invalid Leadership: " + v);
                    continue;
                }
                if (k_up == "LEADERSHIP_GREEN" || k_up == "LEADERSHIPGREEN") {
                    if (auto dv = parse_number(v)) out.meta.leadership_green = *dv;
                    else out.errors.push_back(path + ":" + std::to_string(line_no) + " invalid LeadershipGreen: " + v);
                    continue;
                }

                if (k_up == "ATTACK") {
                    if (auto iv = parse_int(v)) out.meta.attack = *iv;
                    else out.errors.push_back(path + ":" + std::to_string(line_no) + " invalid Attack: " + v);
                    continue;
                }
                if (k_up == "ATTACK_GREEN" || k_up == "ATTACKGREEN") {
                    if (auto dv = parse_number(v)) out.meta.attack_green = *dv;
                    else out.errors.push_back(path + ":" + std::to_string(line_no) + " invalid AttackGreen: " + v);
                    continue;
                }

                if (k_up == "DEFENSE") {
                    if (auto iv = parse_int(v)) out.meta.defense = *iv;
                    else out.errors.push_back(path + ":" + std::to_string(line_no) + " invalid Defense: " + v);
                    continue;
                }
                if (k_up == "DEFENSE_GREEN" || k_up == "DEFENSEGREEN") {
                    if (auto dv = parse_number(v)) out.meta.defense_green = *dv;
                    else out.errors.push_back(path + ":" + std::to_string(line_no) + " invalid DefenseGreen: " + v);
                    continue;
                }

                if (k_up == "POLITICS") {
                    if (auto iv = parse_int(v)) out.meta.politics = *iv;
                    else out.errors.push_back(path + ":" + std::to_string(line_no) + " invalid Politics: " + v);
                    continue;
                }
                if (k_up == "POLITICS_GREEN" || k_up == "POLITICSGREEN") {
                    if (auto dv = parse_number(v)) out.meta.politics_green = *dv;
                    else out.errors.push_back(path + ":" + std::to_string(line_no) + " invalid PoliticsGreen: " + v);
                    continue;
                }

                // Unknown KV line: ignore
            }
        }

        // Context header
        if (parse_header_context(t, ctx)) {
            continue;
        }

        // Stat line
        if (auto st = parse_stat_line(t)) {
            StatOccurrenceV2 occ;
            occ.raw_key = std::move(st->key);
            occ.value = st->value;

            occ.context_type = ctx.type.empty() ? "Unknown" : ctx.type;
            occ.context_name = ctx.name;

            if (occ.context_type == "Specialty") occ.level = parse_specialty_level_from_context_name(occ.context_name);
            else occ.level = std::nullopt;

            occ.is_total = false;
            occ.file_path = path;
            occ.line_number = line_no;
            occ.raw_line = t;

            // Final safety wall
            if (!is_banned_raw_key(occ.raw_key)) {
                out.occurrences.push_back(std::move(occ));
            }
            continue;
        }

        // Everything else: ignore
    }

    // Fallback name from filename
    if (out.meta.name.empty()) {
        std::string name = path;
        auto slash = name.find_last_of("/\\");
        if (slash != std::string::npos) name = name.substr(slash + 1);
        auto dot = name.rfind('.');
        if (dot != std::string::npos) name = name.substr(0, dot);
        out.meta.name = name;
    }

    return out;
}

#include "GeneralLoaderV2.h"

#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

static std::string trim(std::string s)
{
    while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    return s;
}

static bool starts_with(const std::string& s, const std::string& p)
{
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

static std::optional<int> parse_int_after(const std::string& s, const std::string& key)
{
    auto pos = s.find(key);
    if (pos == std::string::npos) return std::nullopt;
    pos += key.size();
    while (pos < s.size() && std::isspace((unsigned char)s[pos])) pos++;
    bool neg = false;
    if (pos < s.size() && s[pos] == '-') { neg = true; pos++; }
    int v = 0;
    bool any = false;
    while (pos < s.size() && std::isdigit((unsigned char)s[pos])) {
        any = true;
        v = v * 10 + (s[pos] - '0');
        pos++;
    }
    if (!any) return std::nullopt;
    return neg ? -v : v;
}

static std::optional<double> parse_double_after(const std::string& s, const std::string& key)
{
    auto pos = s.find(key);
    if (pos == std::string::npos) return std::nullopt;
    pos += key.size();
    while (pos < s.size() && std::isspace((unsigned char)s[pos])) pos++;
    std::string num;
    while (pos < s.size() && (std::isdigit((unsigned char)s[pos]) || s[pos] == '.' || s[pos] == '-' || s[pos] == '+')) {
        num.push_back(s[pos]);
        pos++;
    }
    if (num.empty()) return std::nullopt;
    try {
        return std::stod(num);
    } catch (...) {
        return std::nullopt;
    }
}

static bool is_stat_line(const std::string& line)
{
    // e.g. Key +10.0 or Key -35
    // We'll treat as: first token is key, second token is numeric with sign
    std::istringstream iss(line);
    std::string k, v;
    if (!(iss >> k >> v)) return false;
    if (k.empty()) return false;
    if (v.empty()) return false;
    char c0 = v[0];
    return (c0 == '+' || c0 == '-' || std::isdigit((unsigned char)c0));
}

static std::pair<std::string, double> parse_stat_kv(const std::string& line)
{
    std::istringstream iss(line);
    std::string k, v;
    iss >> k >> v;
    double val = 0.0;
    try { val = std::stod(v); } catch (...) { val = 0.0; }
    return {k, val};
}

// ------------------------------
// Specialty auto-expansion
// ------------------------------
static std::optional<std::vector<int>> split_total_abs_to_increments(double abs_total)
{
    // Match by integer totals. Your patterns:
    // 10 => 1,1,2,2,4
    // 6  => 1,1,1,1,2
    // 50 => 5,5,10,10,20
    // 16 => 2,2,3,3,6
    // 26 => 3,3,5,5,10
    // 35 => 3,4,6,7,15
    // 46 => 5,5,9,9,18
    // Extend here as you discover more.
    static const std::unordered_map<int, std::vector<int>> patterns = {
        { 10, {1,1,2,2,4} },
        {  6, {1,1,1,1,2} },
        { 50, {5,5,10,10,20} },
        { 16, {2,2,3,3,6} },
        { 26, {3,3,5,5,10} },
        { 35, {3,4,6,7,15} },
        { 46, {5,5,9,9,18} },
    };

    int t = (int)std::lround(abs_total);
    auto it = patterns.find(t);
    if (it == patterns.end()) return std::nullopt;
    return it->second;
}

static void expand_specialty_totals_in_place(std::vector<StatOccurrenceV2>& occ)
{
    // Group by context_name within Specialty.
    // Only expand when:
    // - there exists at least one (level=5, is_total=1) row in that specialty
    // - and there are zero rows for levels 1..4 in that specialty (any stat)
    //
    // Add generated L1..L5 increment rows (is_total=0), keep original totals.
    struct Key {
        std::string name;
        bool operator==(const Key& o) const { return name == o.name; }
    };
    struct KeyHash {
        size_t operator()(const Key& k) const { return std::hash<std::string>{}(k.name); }
    };

    std::unordered_map<Key, std::vector<size_t>, KeyHash> idx_by_specialty;
    idx_by_specialty.reserve(64);

    for (size_t i = 0; i < occ.size(); i++) {
        if (occ[i].context_type != "Specialty") continue;
        idx_by_specialty[{occ[i].context_name}].push_back(i);
    }

    std::vector<StatOccurrenceV2> to_add;
    to_add.reserve(256);

    for (auto& kv : idx_by_specialty) {
        const auto& indices = kv.second;

        bool has_l1_4 = false;
        std::vector<size_t> l5_totals;
        for (size_t idx : indices) {
            if (occ[idx].level.has_value()) {
                int lv = *occ[idx].level;
                if (lv >= 1 && lv <= 4) { has_l1_4 = true; break; }
                if (lv == 5 && occ[idx].is_total) l5_totals.push_back(idx);
            }
        }
        if (has_l1_4) continue;
        if (l5_totals.empty()) continue;

        for (size_t idx : l5_totals) {
            const auto& o = occ[idx];

            double v = o.value;
            double abs_total = std::fabs(v);
            auto inc_opt = split_total_abs_to_increments(abs_total);
            if (!inc_opt) continue; // unknown total; leave as-is

            const auto& inc = *inc_opt; // 5 ints
            double sign = (v < 0) ? -1.0 : 1.0;

            for (int lv = 1; lv <= 5; lv++) {
                StatOccurrenceV2 g = o;
                g.level = lv;
                g.is_total = false; // these are increments
                g.value = sign * (double)inc[lv - 1];
                g.raw_line = "# AUTO_EXPANDED_FROM_L5_TOTAL: " + o.raw_line;
                to_add.push_back(std::move(g));
            }
        }
    }

    if (!to_add.empty()) {
        occ.insert(occ.end(), to_add.begin(), to_add.end());
    }
}

// ------------------------------

LoadedGeneralV2 load_general_v2_from_file(const std::string& path)
{
    LoadedGeneralV2 out;
    out.meta.role = "Unknown";

    std::ifstream f(path);
    if (!f.is_open()) {
        out.errors.push_back("Could not open file: " + path);
        return out;
    }

    std::string line;
    int line_no = 0;

    std::string cur_context_type;
    std::string cur_context_name;
    std::optional<int> cur_level;
    bool cur_is_total = false;

    bool in_source_text = false;
    std::ostringstream source_text_buf;

    auto push_occ = [&](const std::string& raw_key, double value, const std::string& raw_line) {
        StatOccurrenceV2 o;
        o.raw_key = raw_key;
        o.value = value;
        o.context_type = cur_context_type;
        o.context_name = cur_context_name;
        o.level = cur_level;
        o.is_total = cur_is_total;
        o.file_path = path;
        o.line_number = line_no;
        o.raw_line = raw_line;
        out.occurrences.push_back(std::move(o));
    };

    while (std::getline(f, line)) {
        line_no++;
        std::string t = trim(line);

        if (t.empty()) continue;

        // Capture verbatim source text block (if present)
        if (starts_with(t, "# SOURCE TEXT (VERBATIM)")) {
            in_source_text = true;
            source_text_buf.str("");
            source_text_buf.clear();
            continue;
        }
        if (in_source_text) {
            // We stop only if another GENERAL starts (or file ends).
            if (starts_with(t, "GENERAL:")) {
                in_source_text = false;
                // fallthrough to handle GENERAL
            } else {
                source_text_buf << line << "\n";
                continue;
            }
        }

        if (starts_with(t, "GENERAL:")) {
            out.meta.name = trim(t.substr(std::string("GENERAL:").size()));
            continue;
        }
        if (starts_with(t, "ROLE:")) {
            out.meta.role = trim(t.substr(std::string("ROLE:").size()));
            continue;
        }
        if (starts_with(t, "IN_TAVERN:")) {
            auto v = trim(t.substr(std::string("IN_TAVERN:").size()));
            out.meta.in_tavern = (v == "true" || v == "1" || v == "yes");
            continue;
        }
        if (starts_with(t, "BASE SKILL |")) {
            out.meta.base_skill_name = trim(t.substr(std::string("BASE SKILL |").size()));
            continue;
        }

        // Flags (optional)
        if (starts_with(t, "double_checked_in_game:")) {
            auto v = trim(t.substr(std::string("double_checked_in_game:").size()));
            out.meta.double_checked_in_game = (v == "1" || v == "true" || v == "yes");
            continue;
        }

        // Core stats
        if (starts_with(t, "Leadership:")) {
            auto base = parse_int_after(t, "Leadership:");
            auto gr   = parse_double_after(t, "(");
            if (base) out.meta.leadership = *base;
            if (gr)   out.meta.leadership_green = *gr;
            continue;
        }
        if (starts_with(t, "Attack:")) {
            auto base = parse_int_after(t, "Attack:");
            auto gr   = parse_double_after(t, "(");
            if (base) out.meta.attack = *base;
            if (gr)   out.meta.attack_green = *gr;
            continue;
        }
        if (starts_with(t, "Defense:")) {
            auto base = parse_int_after(t, "Defense:");
            auto gr   = parse_double_after(t, "(");
            if (base) out.meta.defense = *base;
            if (gr)   out.meta.defense_green = *gr;
            continue;
        }
        if (starts_with(t, "Politics:")) {
            auto base = parse_int_after(t, "Politics:");
            auto gr   = parse_double_after(t, "(");
            if (base) out.meta.politics = *base;
            if (gr)   out.meta.politics_green = *gr;
            continue;
        }

        // Section headers
        if (starts_with(t, "BASE SKILL STATS")) {
            cur_context_type = "BaseSkill";
            cur_context_name = out.meta.base_skill_name;
            cur_level.reset();
            cur_is_total = false;
            continue;
        }

        if (starts_with(t, "ASCENSION")) {
            cur_context_type = "Ascension";
            cur_context_name = t; // "ASCENSION 1", etc.
            cur_level.reset();
            cur_is_total = false;
            continue;
        }

        if (starts_with(t, "COVENANT")) {
            cur_context_type = "Covenant";
            cur_context_name = t;
            cur_level.reset();
            cur_is_total = false;
            continue;
        }

        // Specialty headers like:
        // "Specialty 1 L1"
        // "Specialty 4 | Queen of Poland L5 (TOTAL)"
        if (starts_with(t, "Specialty")) {
            cur_context_type = "Specialty";
            cur_context_name = t;

            // Parse level
            cur_level.reset();
            cur_is_total = false;

            // crude parse: look for " L<number>"
            auto lpos = t.find(" L");
            if (lpos != std::string::npos && lpos + 2 < t.size()) {
                int lv = 0;
                size_t p = lpos + 2;
                while (p < t.size() && std::isdigit((unsigned char)t[p])) {
                    lv = lv * 10 + (t[p] - '0');
                    p++;
                }
                if (lv > 0) cur_level = lv;
            }

            if (t.find("(TOTAL)") != std::string::npos) cur_is_total = true;
            continue;
        }

        // Stat line
        if (!cur_context_type.empty() && is_stat_line(t)) {
            auto kv = parse_stat_kv(t);
            if (!kv.first.empty()) {
                push_occ(kv.first, kv.second, t);
            }
            continue;
        }
    }

    // save verbatim
    out.meta.source_text_verbatim = source_text_buf.str();

    // ✅ NEW: auto-expand specialty L5 totals using known patterns
    expand_specialty_totals_in_place(out.occurrences);

    if (out.meta.name.empty()) {
        out.errors.push_back("Missing GENERAL: line");
    }

    return out;
}

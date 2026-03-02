#include "GeneralLoader.h"
#include "StatParse.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>

static std::string trim(std::string s) {
    auto not_space = [](unsigned char c){ return !std::isspace(c); };
    while (!s.empty() && !not_space((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && !not_space((unsigned char)s.back())) s.pop_back();
    return s;
}

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

static double parse_number(std::string token) {
    token = trim(token);
    if (!token.empty() && token.back() == '%') token.pop_back(); // allow "10%"
    return std::stod(token); // handles +10, -5, 12.5, etc.
}

GeneralDefinition load_general_from_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Failed to open file: " + path);

    GeneralDefinition def;

    enum class Mode { None, Base, Asc, Spec, Cov };
    Mode mode = Mode::None;

    int ascLevel = 0;
    int specIdx = -1;   // 0..3
    int specLvl = -1;   // 0..4

    int covenantNum = 0; // 1..6 (assigned by order encountered)

    std::string line;
    int lineNo = 0;

    auto push_mod = [&](Stat stat, double value, const std::string& source) {
        Modifier m{stat, kind_for_stat(stat), value, source};

        if (mode == Mode::Asc) {
            def.ascension[ascLevel].push_back(std::move(m));
        } else if (mode == Mode::Spec) {
            def.specialties.at(specIdx).at(specLvl).push_back(std::move(m));
        } else if (mode == Mode::Cov) {
            def.covenants[covenantNum].push_back(std::move(m));
        } else {
            def.base.push_back(std::move(m));
        }
    };

    while (std::getline(in, line)) {
        ++lineNo;
        line = trim(line);
        if (line.empty()) continue;
        if (starts_with(line, "#")) continue;

        if (starts_with(line, "GENERAL:")) {
            def.name = trim(line.substr(std::string("GENERAL:").size()));
            mode = Mode::None;
            continue;
        }
        if (starts_with(line, "ROLE:")) {
            def.role = trim(line.substr(std::string("ROLE:").size()));
            mode = Mode::None;
            continue;
        }
        if (line == "BASE:") {
            mode = Mode::Base;
            continue;
        }
        if (starts_with(line, "ASCENSION")) {
            std::string tmp = line;
            if (!tmp.empty() && tmp.back() == ':') tmp.pop_back();

            std::istringstream iss(tmp);
            std::string word;
            iss >> word;      // ASCENSION
            iss >> ascLevel;  // number
            if (!iss) throw std::runtime_error("Bad ASCENSION header at line " + std::to_string(lineNo));
            mode = Mode::Asc;
            continue;
        }
        if (starts_with(line, "SPECIALTY")) {
            // Accept:
            //  "SPECIALTY 2 L5:"  (level header)
            // Ignore:
            //  "SPECIALTY 2 | Name" (pretty title)
            if (line.find('|') != std::string::npos) {
                continue;
            }

            std::string tmp = line;
            if (!tmp.empty() && tmp.back() == ':') tmp.pop_back();

            std::istringstream iss(tmp);
            std::string word, L;
            int sIdx;
            iss >> word;   // SPECIALTY
            iss >> sIdx;   // 1..4
            iss >> L;      // L1..L5
            if (!iss || sIdx < 1 || sIdx > 4 || L.size() < 2 || (L[0] != 'L' && L[0] != 'l')) {
                throw std::runtime_error("Bad SPECIALTY header at line " + std::to_string(lineNo));
            }

            int lvl = std::stoi(L.substr(1)); // 1..5
            if (lvl < 1 || lvl > 5) {
                throw std::runtime_error("Bad SPECIALTY level at line " + std::to_string(lineNo));
            }

            specIdx = sIdx - 1;
            specLvl = lvl - 1;
            mode = Mode::Spec;
            continue;
        }

        if (starts_with(line, "COVENANT")) {
            // Example:
            // "COVENANT War"
            // Assign covenant numbers in order encountered: 1..6
            std::string tmp = line;
            std::istringstream iss(tmp);
            std::string word;
            iss >> word; // COVENANT
            std::string covName;
            std::getline(iss, covName);
            covName = trim(covName);

            covenantNum += 1;
            if (covenantNum > 6) covenantNum = 6; // clamp (won't crash)

            def.covenantNames[covenantNum] = covName.empty() ? ("Covenant " + std::to_string(covenantNum)) : covName;
            mode = Mode::Cov;
            continue;
        }

        // Parse stat lines: Key +Value
        std::istringstream iss(line);
        std::string key, valTok;
        iss >> key >> valTok;
        if (!iss) continue;

        auto statOpt = stat_from_key(key);
        if (!statOpt.has_value()) {
            // Unknown stat -> ignore for v1
            continue;
        }

        double value = parse_number(valTok);

        std::string source;
        if (mode == Mode::Asc) source = "Ascension " + std::to_string(ascLevel);
        else if (mode == Mode::Spec) source = "Specialty " + std::to_string(specIdx + 1) + " L" + std::to_string(specLvl + 1);
        else if (mode == Mode::Cov) {
            auto it = def.covenantNames.find(covenantNum);
            source = "Covenant " + std::to_string(covenantNum) + (it != def.covenantNames.end() ? (": " + it->second) : "");
        } else source = "Base";

        push_mod(*statOpt, value, source);
    }

    if (def.name.empty()) throw std::runtime_error("Missing GENERAL: header in " + path);
    if (def.role.empty()) def.role = "Unknown";
    return def;
}

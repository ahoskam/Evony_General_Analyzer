// SpecialtyExpander.cpp
//
// Expands SPECIALTY blocks that are currently totals-only:
//
//   SPECIALTY 1 L5:
//   SomeStatPct 10
//   AnotherStatPct -20
//
// into:
//
//   SPECIALTY 1 L1:
//   SomeStatPct 1
//   AnotherStatPct -2
//
//   SPECIALTY 1 L2:
//   ...
//   SPECIALTY 1 L5:
//   ...
//
// Notes:
// - Only expands blocks that contain "SPECIALTY <n> L5:" and DO NOT already contain L1..L4.
// - Skips any files under a "/_bak/" directory to avoid backing up backups forever.
// - Preserves "# SPECIALTY_#_L5_TEXT:" blocks and other non-stat lines.
// - For totals without a known split pattern, it leaves the L5 value unchanged and reports it.
//
// Build:
//   g++ -std=c++17 -O2 -o src/Expand_Specialty/specialty_expander src/Expand_Specialty/SpecialtyExpander.cpp
//
// Run:
//   ./src/Expand_Specialty/specialty_expander <src_dir> <dst_dir> <bak_dir>
//
// Example (recommended workflow):
//   mkdir -p data/import
//   cp -a data/imported/*.txt data/import/
//   ./src/Expand_Specialty/specialty_expander data/import data/import data/import/_bak
//   ./build/importer_v2 --db data/evony_v2.db --path data/import
//

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>   // <-- std::llround, std::round, std::fabs

namespace fs = std::filesystem;

static std::string trim(const std::string& s) {
  size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
  return s.substr(b, e - b);
}

static bool starts_with(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static bool contains_path_component_bak(const fs::path& p) {
  for (const auto& part : p) {
    if (part == "_bak") return true;
  }
  return false;
}

static std::vector<std::string> read_all_lines(const fs::path& path) {
  std::ifstream in(path);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(in, line)) {
    lines.push_back(line);
  }
  return lines;
}

static bool write_all_lines_atomic(const fs::path& path, const std::vector<std::string>& lines) {
  fs::path tmp = path;
  tmp += ".tmp";

  std::ofstream out(tmp);
  if (!out) return false;

  for (size_t i = 0; i < lines.size(); i++) {
    out << lines[i];
    if (i + 1 < lines.size()) out << "\n";
  }
  out.close();
  if (!out) return false;

  std::error_code ec;
  fs::rename(tmp, path, ec);
  if (ec) {
    std::error_code ec2;
    fs::remove(path, ec2);
    ec.clear();
    fs::rename(tmp, path, ec);
    if (ec) {
      fs::remove(tmp, ec2);
      return false;
    }
  }
  return true;
}

static fs::path unique_backup_path(const fs::path& bak_dir, const fs::path& rel) {
  fs::path target = bak_dir / rel.filename();
  if (!fs::exists(target)) return target;

  for (int i = 1; i < 10000; i++) {
    fs::path candidate =
        bak_dir / (rel.stem().string() + ".bak" + std::to_string(i) + rel.extension().string());
    if (!fs::exists(candidate)) return candidate;
  }
  return bak_dir / (rel.stem().string() + ".bak" + rel.extension().string());
}

static bool backup_file(const fs::path& src_file, const fs::path& bak_dir) {
  std::error_code ec;
  fs::create_directories(bak_dir, ec);

  fs::path target = unique_backup_path(bak_dir, src_file.filename());
  fs::copy_file(src_file, target, fs::copy_options::overwrite_existing, ec);
  return !ec;
}

// Parse: "SomeKey 10", "SomeKey -10.0", "SomeKey +6"
static bool parse_stat_line(const std::string& line, std::string& out_key, double& out_val) {
  std::string t = trim(line);
  if (t.empty()) return false;
  if (t[0] == '#') return false;
  if (starts_with(t, "SPECIALTY")) return false;

  std::istringstream iss(t);
  std::vector<std::string> toks;
  std::string tok;
  while (iss >> tok) toks.push_back(tok);
  if (toks.size() < 2) return false;

  const std::string& vtok = toks.back();
  char* endp = nullptr;
  double v = std::strtod(vtok.c_str(), &endp);
  if (endp == vtok.c_str() || *endp != '\0') return false;

  std::ostringstream k;
  for (size_t i = 0; i + 1 < toks.size(); i++) {
    if (i) k << " ";
    k << toks[i];
  }
  out_key = k.str();
  out_val = v;
  return true;
}

// Known split patterns for ABS(L5) totals -> [L1,L2,L3,L4,L5].
static std::optional<std::vector<double>> split_total_abs(double abs_total) {
  int t = static_cast<int>(std::llround(abs_total));

  static const std::map<int, std::vector<double>> kSplits = {
      {6, {1, 1, 1, 1, 2}},
      {8, {1, 1, 2, 2, 2}},
      {10, {1, 1, 2, 2, 4}},
      {15, {2, 2, 3, 3, 5}},
      {16, {2, 2, 3, 3, 6}},    // user-provided
      {20, {2, 2, 4, 4, 8}},
      {25, {3, 3, 5, 5, 9}},    // adjust if your canonical says otherwise
      {26, {3, 3, 5, 5, 10}},   // user-provided
      {30, {3, 3, 6, 6, 12}},
      {35, {3, 4, 6, 7, 15}},   // user-provided
      {36, {4, 4, 7, 7, 14}},   // adjust if your canonical says otherwise
      {40, {4, 4, 8, 8, 16}},
      {45, {5, 5, 9, 9, 17}},   // adjust if your canonical says otherwise
      {46, {5, 5, 9, 9, 18}},   // user-provided
      {50, {5, 5, 10, 10, 20}},
      {80, {10, 10, 15, 15, 30}},
      {100, {15, 15, 20, 20, 30}},
  };

  auto it = kSplits.find(t);
  if (it == kSplits.end()) return std::nullopt;
  return it->second;
}

// Matches: "SPECIALTY 1 L5:" etc.
static std::optional<std::pair<int, int>> parse_specialty_level_header(const std::string& line) {
  static const std::regex re(R"(^\s*SPECIALTY\s+([1-4])\s+L([1-5])\s*:\s*$)");
  std::smatch m;
  if (std::regex_match(line, m, re)) {
    int spec = std::stoi(m[1].str());
    int lvl = std::stoi(m[2].str());
    return std::make_pair(spec, lvl);
  }
  return std::nullopt;
}

static bool is_specialty_text_marker(const std::string& line) {
  static const std::regex re(R"(^\s*#\s*SPECIALTY_[1-4]_L5_TEXT\s*:\s*$)");
  return std::regex_match(line, re);
}

struct ExpandReport {
  int blocks_seen = 0;
  int blocks_already_expanded = 0;
  int blocks_expanded = 0;
  int blocks_left_unknown = 0;

  std::map<int, int> unknown_totals_count;
};

static void expand_specialty_block(std::vector<std::string>& out,
                                   int spec_num,
                                   const std::vector<std::string>& block_lines,
                                   ExpandReport& rep) {
  rep.blocks_seen++;

  struct StatLine {
    std::string key;
    double val;
    std::string original;
    bool parsed;
  };

  std::vector<StatLine> stats;
  bool any_l1_marker = false;

  for (const auto& bl : block_lines) {
    auto hdr = parse_specialty_level_header(bl);
    if (hdr && hdr->first == spec_num && hdr->second >= 1 && hdr->second <= 4) {
      any_l1_marker = true;
    }

    std::string k;
    double v = 0;
    if (parse_stat_line(bl, k, v)) {
      stats.push_back({k, v, bl, true});
    } else {
      stats.push_back({"", 0.0, bl, false});
    }
  }

  if (any_l1_marker) {
    rep.blocks_already_expanded++;
    for (const auto& bl : block_lines) out.push_back(bl);
    return;
  }

  // If ANY parsed stat total is unknown, leave whole block unchanged (no mixed expansions).
  std::set<int> unknown_abs;
  for (const auto& st : stats) {
    if (!st.parsed) continue;
    double abs_total = std::fabs(st.val);
    auto split = split_total_abs(abs_total);
    if (!split.has_value()) {
      unknown_abs.insert(static_cast<int>(std::llround(abs_total)));
    }
  }

  if (!unknown_abs.empty()) {
    rep.blocks_left_unknown++;
    for (int u : unknown_abs) rep.unknown_totals_count[u]++;
    for (const auto& bl : block_lines) out.push_back(bl);
    return;
  }

  rep.blocks_expanded++;

  auto emit_level = [&](int level, const std::vector<std::string>& lines) {
    out.push_back("SPECIALTY " + std::to_string(spec_num) + " L" + std::to_string(level) + ":");
    for (const auto& l : lines) out.push_back(l);
    out.push_back("");
  };

  std::vector<std::string> lvl_lines[6];  // 1..5

  for (const auto& st : stats) {
    if (!st.parsed) {
      lvl_lines[5].push_back(st.original);
      continue;
    }

    double sign = (st.val < 0) ? -1.0 : 1.0;
    double abs_total = std::fabs(st.val);
    auto split = split_total_abs(abs_total).value();

    for (int lvl = 1; lvl <= 5; lvl++) {
      double v = sign * split[lvl - 1];
      std::ostringstream oss;
      oss << st.key << " ";
      if (std::fabs(v - std::round(v)) < 1e-9) {
        oss << static_cast<long long>(std::llround(v));
      } else {
        oss << std::fixed << std::setprecision(2) << v;
      }
      lvl_lines[lvl].push_back(oss.str());
    }
  }

  for (int lvl = 1; lvl <= 5; lvl++) emit_level(lvl, lvl_lines[lvl]);

  while (!out.empty() && trim(out.back()).empty()) out.pop_back();
}

static bool process_file(const fs::path& src_file,
                         const fs::path& dst_file,
                         const fs::path& bak_dir,
                         ExpandReport& rep) {
  auto lines = read_all_lines(src_file);

  std::vector<std::string> out;
  out.reserve(lines.size() + 128);

  bool changed = false;

  for (size_t i = 0; i < lines.size(); i++) {
    const std::string& l = lines[i];
    auto hdr = parse_specialty_level_header(l);

    // Preserve "# SPECIALTY_#_L5_TEXT:" blocks unchanged.
    if (is_specialty_text_marker(l)) {
      out.push_back(l);
      size_t j = i + 1;
      for (; j < lines.size(); j++) {
        const std::string& lj = lines[j];
        if (is_specialty_text_marker(lj)) break;
        if (parse_specialty_level_header(lj).has_value()) break;
        std::string tj = trim(lj);
        if (tj == "COVENANTS" || starts_with(tj, "# SOURCE") || tj == "# NEW STAT KEYS NOTE") break;
        out.push_back(lj);
      }
      i = j - 1;
      continue;
    }

    // Only act on "SPECIALTY X L5:" headers.
    if (!hdr || hdr->second != 5) {
      out.push_back(l);
      continue;
    }

    int spec_num = hdr->first;

    // Gather block lines following the L5 header.
    std::vector<std::string> block;
    size_t j = i + 1;
    for (; j < lines.size(); j++) {
      const std::string& lj = lines[j];
      if (parse_specialty_level_header(lj).has_value()) break;
      if (is_specialty_text_marker(lj)) break;

      std::string tj = trim(lj);
      if (tj == "COVENANTS" || starts_with(tj, "# SOURCE") || tj == "# NEW STAT KEYS NOTE") break;

      block.push_back(lj);
    }

    // Safety: if later we already have L1..L4 for this spec, just copy.
    bool already_expanded = false;
    for (size_t k = i; k < std::min(lines.size(), i + 200); k++) {
      auto h2 = parse_specialty_level_header(lines[k]);
      if (h2 && h2->first == spec_num && h2->second >= 1 && h2->second <= 4) {
        already_expanded = true;
        break;
      }
      auto h5 = parse_specialty_level_header(lines[k]);
      if (h5 && h5->first != spec_num && h5->second == 5) break;
    }

    if (already_expanded) {
      rep.blocks_already_expanded++;
      out.push_back(l);
      for (const auto& bl : block) out.push_back(bl);
    } else {
      // Try expand. If it expands, we replace; otherwise keep original header+block.
      std::vector<std::string> temp_out;
      ExpandReport before = rep;

      expand_specialty_block(temp_out, spec_num, block, rep);

      if (rep.blocks_expanded > before.blocks_expanded) {
        changed = true;
        for (const auto& tl : temp_out) out.push_back(tl);
      } else {
        out.push_back(l);
        for (const auto& bl : block) out.push_back(bl);
      }
    }

    i = j - 1;
  }

  fs::create_directories(dst_file.parent_path());

  if (src_file == dst_file) {
    if (changed) {
      if (!backup_file(src_file, bak_dir)) {
        std::cerr << "[WARN] backup failed for " << src_file << "\n";
      }
      if (!write_all_lines_atomic(dst_file, out)) return false;
    }
    return true;
  } else {
    if (!changed) {
      std::error_code ec;
      fs::copy_file(src_file, dst_file, fs::copy_options::overwrite_existing, ec);
      return !ec;
    } else {
      if (!backup_file(src_file, bak_dir)) {
        std::cerr << "[WARN] backup failed for " << src_file << "\n";
      }
      return write_all_lines_atomic(dst_file, out);
    }
  }
}

static void usage(const char* argv0) {
  std::cerr << "Usage:\n  " << argv0 << " <src_dir> <dst_dir> <bak_dir>\n";
}

int main(int argc, char** argv) {
  if (argc != 4) {
    usage(argv[0]);
    return 2;
  }

  fs::path src_dir = fs::path(argv[1]);
  fs::path dst_dir = fs::path(argv[2]);
  fs::path bak_dir = fs::path(argv[3]);

  if (!fs::exists(src_dir) || !fs::is_directory(src_dir)) {
    std::cerr << "Source directory not found: " << src_dir << "\n";
    return 2;
  }

  std::error_code ec;
  fs::create_directories(dst_dir, ec);
  fs::create_directories(bak_dir, ec);

  ExpandReport rep;

  int files_seen = 0;
  int files_processed = 0;
  int files_failed = 0;

  for (const auto& ent : fs::recursive_directory_iterator(
           src_dir, fs::directory_options::skip_permission_denied)) {
    if (!ent.is_regular_file()) continue;

    fs::path p = ent.path();

    // Skip backup trees
    if (contains_path_component_bak(p)) continue;

    if (p.extension() != ".txt") continue;
    files_seen++;

    fs::path rel = fs::relative(p, src_dir, ec);
    if (ec) rel = p.filename();

    // Flat output into dst_dir (matches your current workflow)
    fs::path out_path = dst_dir / rel.filename();

    if (!process_file(p, out_path, bak_dir, rep)) {
      files_failed++;
      std::cerr << "[ERROR] Failed processing " << p << "\n";
      continue;
    }
    files_processed++;
  }

  std::cout << "SpecialtyExpander finished.\n";
  std::cout << "  Files seen: " << files_seen << "\n";
  std::cout << "  Files processed: " << files_processed << "\n";
  std::cout << "  Files failed: " << files_failed << "\n";
  std::cout << "  Specialty L5 blocks seen: " << rep.blocks_seen << "\n";
  std::cout << "  Blocks already expanded: " << rep.blocks_already_expanded << "\n";
  std::cout << "  Blocks expanded: " << rep.blocks_expanded << "\n";
  std::cout << "  Blocks left unexpanded (unknown totals): " << rep.blocks_left_unknown << "\n";

  if (!rep.unknown_totals_count.empty()) {
    std::cout << "  Unknown L5 totals encountered (ABS):\n";
    for (const auto& kv : rep.unknown_totals_count) {
      std::cout << "    " << kv.first << "  (blocks: " << kv.second << ")\n";
    }
    std::cout << "  Edit split_total_abs() to add patterns for these totals.\n";
  }

  return (files_failed == 0) ? 0 : 1;
}

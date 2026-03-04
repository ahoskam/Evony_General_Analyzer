#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

inline std::string trim_copy(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(),
          std::find_if(s.begin(), s.end(),
                       [&](char c) { return not_space((unsigned char)c); }));
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [&](char c) { return not_space((unsigned char)c); })
              .base(),
          s.end());
  return s;
}

inline std::string lower_copy(std::string s) {
  for (char &c : s) {
    c = (char)std::tolower((unsigned char)c);
  }
  return s;
}

inline bool starts_with(const std::string &s, const std::string &prefix) {
  return s.size() >= prefix.size() &&
         s.compare(0, prefix.size(), prefix) == 0;
}

inline constexpr std::array<std::string_view, 10> kValidGeneralRoles = {
    "Ground", "Mounted", "Ranged", "Siege", "Defense",
    "Mixed",  "Admin",   "Duty",   "Mayor", "Unknown"};

inline std::string normalize_general_role(const std::string &raw) {
  std::string role = lower_copy(trim_copy(raw));
  if (role.empty()) {
    return "Unknown";
  }

  if (starts_with(role, "admin")) {
    return "Admin";
  }
  if (role == "ground") {
    return "Ground";
  }
  if (role == "mounted") {
    return "Mounted";
  }
  if (role == "ranged") {
    return "Ranged";
  }
  if (role == "siege") {
    return "Siege";
  }
  if (role == "defense") {
    return "Defense";
  }
  if (role == "mixed") {
    return "Mixed";
  }
  if (role == "duty") {
    return "Duty";
  }
  if (role == "mayor") {
    return "Mayor";
  }
  if (role == "unknown") {
    return "Unknown";
  }
  return "Unknown";
}

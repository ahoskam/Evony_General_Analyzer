#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cmath>

inline bool str_icontains(const std::string& hay, const std::string& needle) {
  if (needle.empty()) return true;
  auto H = hay;
  auto N = needle;
  for (auto& c : H) c = (char)std::tolower((unsigned char)c);
  for (auto& c : N) c = (char)std::tolower((unsigned char)c);
  return H.find(N) != std::string::npos;
}

inline bool nearly_equal(double a, double b, double eps = 1e-9) {
  return std::fabs(a - b) <= eps;
}

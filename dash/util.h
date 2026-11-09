#pragma once
// String helpers with no ESPHome dependency.
#include <string>
#include <vector>

namespace dash {

static std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  size_t start = 0;
  while (true) {
    size_t pos = s.find(sep, start);
    out.push_back(s.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
    if (pos == std::string::npos)
      break;
    start = pos + 1;
  }
  return out;
}

static std::vector<std::vector<std::string>> records(const std::string &s, size_t minFields) {
  std::vector<std::vector<std::string>> out;
  for (auto &line : split(s, '\n')) {
    if (line.empty())
      continue;
    auto f = split(line, '|');
    if (f.size() >= minFields)
      out.push_back(f);
  }
  return out;
}

static std::string upper(std::string s) {
  for (auto &c : s)
    if (c >= 'a' && c <= 'z')
      c -= 32;
  return s;
}

} // namespace dash

#include "robots.hpp"

#include "http_client.hpp"

#include <cctype>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace wsf::detail {
namespace {

std::string origin_of(std::string_view url) {
  auto scheme = url.find("://");
  if (scheme == std::string_view::npos) return {};
  auto host_end = url.find('/', scheme + 3);
  if (host_end == std::string_view::npos) return std::string(url);
  return std::string(url.substr(0, host_end));
}

std::string path_of(std::string_view url) {
  auto scheme = url.find("://");
  if (scheme == std::string_view::npos) return "/";
  auto path = url.find('/', scheme + 3);
  if (path == std::string_view::npos) return "/";
  auto hash = url.find('#', path);
  auto q = url.find('?', path);
  size_t end = url.size();
  if (hash != std::string_view::npos) end = hash;
  if (q != std::string_view::npos && q < end) end = q;
  return std::string(url.substr(path, end - path));
}

bool path_match(std::string_view rule_path, std::string_view url_path) {
  if (rule_path.empty()) return true;
  return url_path.rfind(rule_path, 0) == 0;
}

struct RobotsRules {
  std::vector<std::string> allows;
  std::vector<std::string> disallows;
};

RobotsRules parse_robots(std::string_view body) {
  RobotsRules rules;
  bool in_star = false;

  auto flush_line = [&](std::string_view raw) {
    auto cmt = raw.find('#');
    if (cmt != std::string_view::npos) raw = raw.substr(0, cmt);
    while (!raw.empty() && (raw.back() == '\r' || raw.back() == ' ' || raw.back() == '\t'))
      raw.remove_suffix(1);
    size_t i = 0;
    while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\t')) ++i;
    raw.remove_prefix(i);
    if (raw.empty()) return;
    auto colon = raw.find(':');
    if (colon == std::string_view::npos) return;
    auto key = raw.substr(0, colon);
    auto val = raw.substr(colon + 1);
    while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.remove_prefix(1);
    std::string key_l;
    key_l.reserve(key.size());
    for (char ch : key) key_l.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));

    if (key_l == "user-agent") {
      in_star = (val == "*");
    } else if (in_star && key_l == "disallow") {
      rules.disallows.emplace_back(val);
    } else if (in_star && key_l == "allow") {
      rules.allows.emplace_back(val);
    }
  };

  size_t start = 0;
  while (start <= body.size()) {
    auto nl = body.find('\n', start);
    if (nl == std::string_view::npos) {
      flush_line(body.substr(start));
      break;
    }
    flush_line(body.substr(start, nl - start));
    start = nl + 1;
  }
  return rules;
}

bool allowed_by(const RobotsRules& rules, std::string_view path) {
  size_t best_dis = 0;
  bool dis = false;
  for (const auto& d : rules.disallows) {
    if (d.empty()) continue;  // empty Disallow => allow all
    if (path_match(d, path) && d.size() >= best_dis) {
      best_dis = d.size();
      dis = true;
    }
  }
  size_t best_all = 0;
  bool all = false;
  for (const auto& a : rules.allows) {
    if (path_match(a, path) && a.size() >= best_all) {
      best_all = a.size();
      all = true;
    }
  }
  if (all && best_all >= best_dis) return true;
  if (dis) return false;
  return true;
}

std::mutex g_mu;
std::unordered_map<std::string, RobotsRules> g_cache;

}  // namespace

bool robots_allow(std::string_view url, int timeout_ms) {
  auto origin = origin_of(url);
  if (origin.empty()) return true;
  auto path = path_of(url);

  {
    std::lock_guard lock(g_mu);
    auto it = g_cache.find(origin);
    if (it != g_cache.end()) return allowed_by(it->second, path);
  }

  auto resp = http_get(origin + "/robots.txt", timeout_ms);
  RobotsRules rules;
  if (resp.error.empty() && resp.status > 0 && resp.status < 400) {
    rules = parse_robots(resp.body);
  }

  {
    std::lock_guard lock(g_mu);
    g_cache[origin] = rules;
    return allowed_by(rules, path);
  }
}

}  // namespace wsf::detail

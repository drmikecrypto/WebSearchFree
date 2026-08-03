#include "wsf/wsf.hpp"

#include "http_client.hpp"
#include "html_utils.hpp"
#include "robots.hpp"

#include <algorithm>

namespace wsf {

std::string extract(std::string_view url, int timeout_ms) {
  if (!detail::robots_allow(url, std::min(timeout_ms, 3000))) {
    return {};
  }
  auto resp = detail::http_get(url, timeout_ms);
  if (!resp.error.empty() || resp.body.empty()) {
    return {};
  }
  return detail::extract_main_content(resp.body);
}

}  // namespace wsf

#pragma once

#include <string>
#include <string_view>

namespace wsf::detail {

/// Returns true if robots.txt allows fetching `url` for our user-agent (best-effort).
/// On fetch/parse failure, allows the request (fail-open) to avoid blocking useful extracts.
bool robots_allow(std::string_view url, int timeout_ms = 3000);

}  // namespace wsf::detail

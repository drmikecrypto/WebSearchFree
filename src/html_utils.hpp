#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace wsf::detail {

struct SerpHit {
  std::string title;
  std::string url;
  std::string snippet;
  int rank = 0;
  std::string engine;
};

std::string html_unescape(std::string_view input);
std::string strip_tags(std::string_view html);
std::string collapse_whitespace(std::string_view input);
std::string normalize_url(std::string_view url);
std::string url_encode(std::string_view value);
std::string attribute_value(std::string_view tag, std::string_view attr);
std::vector<std::string> find_tag_blocks(std::string_view html, std::string_view open_needle);

/// Extract main textual content from an HTML document.
std::string extract_main_content(std::string_view html);

/// Parse DuckDuckGo HTML SERP.
std::vector<SerpHit> parse_ddg_html(std::string_view html);

/// Parse Brave Search HTML SERP.
std::vector<SerpHit> parse_brave_html(std::string_view html);

}  // namespace wsf::detail

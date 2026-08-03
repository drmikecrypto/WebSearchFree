#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wsf {

struct Options {
  int max_results = 5;
  bool include_raw_content = false;
  std::vector<std::string> engines = {"ddg", "brave", "wikipedia"};
  int timeout_ms = 8000;
  int fetch_concurrency = 4;
};

struct Result {
  std::string title;
  std::string url;
  std::string content;
  double score = 0.0;
  std::optional<std::string> raw_content;
};

struct Response {
  std::string query;
  std::vector<Result> results;
};

/// Metasearch the public web. No API keys required.
Response search(std::string_view query, const Options& options = {});

/// Fetch a URL and extract main content as plain/markdown-ish text.
std::string extract(std::string_view url, int timeout_ms = 8000);

/// Serialize a search response to Tavily-shaped JSON.
nlohmann::json to_json(const Response& response);

}  // namespace wsf

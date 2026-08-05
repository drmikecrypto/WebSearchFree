#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wsf {

inline constexpr const char* kVersion = "0.2.0";

struct Options {
  int max_results = 5;
  bool include_raw_content = false;
  std::vector<std::string> engines = {"ddg", "brave", "wikipedia"};
  int timeout_ms = 8000;
  int fetch_concurrency = 4;
  /// Base URL of a SearXNG instance (e.g. http://127.0.0.1:8080). Used when
  /// "searx" is in engines. Falls back to WSF_SEARX_URL env var.
  std::string searx_url;
};

struct Result {
  std::string title;
  std::string url;
  std::string content;
  double score = 0.0;
  std::optional<std::string> raw_content;
};

struct EngineStatus {
  std::string name;
  bool ok = false;
  int hit_count = 0;
  std::string error;
};

struct Response {
  std::string query;
  std::vector<Result> results;
  std::vector<EngineStatus> engines;
  std::vector<std::string> warnings;
  double response_time = 0.0;  // seconds, Tavily-shaped
};

struct ExtractResult {
  std::string url;
  std::string raw_content;
  /// ok | robots_denied | fetch_failed | empty
  std::string status;
  std::string error;
};

/// Metasearch the public web. No API keys required.
Response search(std::string_view query, const Options& options = {});

/// Fetch a URL and extract main content (structured status).
ExtractResult extract_one(std::string_view url, int timeout_ms = 8000);

/// Parallel multi-URL extract.
std::vector<ExtractResult> extract_many(const std::vector<std::string>& urls,
                                        int timeout_ms = 8000, int concurrency = 4);

/// Fetch a URL and extract main content as plain/markdown-ish text.
/// Returns empty string on failure (prefer extract_one for status).
std::string extract(std::string_view url, int timeout_ms = 8000);

/// Serialize a search response to Tavily-shaped JSON.
nlohmann::json to_json(const Response& response);

/// Serialize extract results.
nlohmann::json to_json(const ExtractResult& result);
nlohmann::json to_json(const std::vector<ExtractResult>& results);

/// Built-in engine names (ddg, brave, wikipedia, searx).
std::vector<std::string> available_engines();

}  // namespace wsf

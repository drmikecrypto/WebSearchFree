#include "wsf/wsf.hpp"

#include "http_client.hpp"
#include "html_utils.hpp"
#include "robots.hpp"

#include <algorithm>
#include <mutex>
#include <thread>

namespace wsf {

ExtractResult extract_one(std::string_view url, int timeout_ms) {
  ExtractResult out;
  out.url = std::string(url);
  if (out.url.empty()) {
    out.status = "empty";
    out.error = "url is empty";
    return out;
  }

  if (!detail::robots_allow(url, std::min(timeout_ms, 3000))) {
    out.status = "robots_denied";
    out.error = "robots.txt disallows fetch";
    return out;
  }

  auto resp = detail::http_get(url, timeout_ms);
  if (!resp.error.empty()) {
    out.status = "fetch_failed";
    out.error = resp.error;
    return out;
  }
  if (resp.status >= 400) {
    out.status = "fetch_failed";
    out.error = "HTTP status " + std::to_string(resp.status);
    return out;
  }
  if (resp.body.empty()) {
    out.status = "empty";
    out.error = "empty response body";
    return out;
  }

  out.raw_content = detail::extract_main_content(resp.body);
  if (out.raw_content.empty()) {
    out.status = "empty";
    out.error = "no extractable content";
    return out;
  }
  out.status = "ok";
  return out;
}

std::vector<ExtractResult> extract_many(const std::vector<std::string>& urls, int timeout_ms,
                                        int concurrency) {
  std::vector<ExtractResult> results(urls.size());
  if (urls.empty()) return results;

  concurrency = std::max(1, concurrency);
  std::mutex mu;
  size_t next = 0;

  auto worker = [&]() {
    for (;;) {
      size_t i = 0;
      {
        std::lock_guard lock(mu);
        if (next >= urls.size()) return;
        i = next++;
      }
      results[i] = extract_one(urls[i], timeout_ms);
    }
  };

  std::vector<std::thread> threads;
  int n = std::min(concurrency, static_cast<int>(urls.size()));
  threads.reserve(static_cast<size_t>(n));
  for (int t = 0; t < n; ++t) threads.emplace_back(worker);
  for (auto& th : threads) th.join();
  return results;
}

std::string extract(std::string_view url, int timeout_ms) {
  return extract_one(url, timeout_ms).raw_content;
}

nlohmann::json to_json(const ExtractResult& result) {
  return nlohmann::json{
      {"url", result.url},
      {"raw_content", result.raw_content},
      {"status", result.status},
      {"error", result.error.empty() ? nullptr : nlohmann::json(result.error)},
  };
}

nlohmann::json to_json(const std::vector<ExtractResult>& results) {
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& r : results) arr.push_back(to_json(r));
  return nlohmann::json{{"results", std::move(arr)}};
}

}  // namespace wsf

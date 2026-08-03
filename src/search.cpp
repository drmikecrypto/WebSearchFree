#include "wsf/wsf.hpp"

#include "engines/engine.hpp"
#include "http_client.hpp"
#include "html_utils.hpp"
#include "rank.hpp"

#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace wsf {
namespace {

struct CacheEntry {
  Response response;
  std::chrono::steady_clock::time_point expires;
};

std::mutex g_cache_mu;
std::unordered_map<std::string, CacheEntry> g_cache;
constexpr auto kCacheTtl = std::chrono::seconds(120);

std::string cache_key(std::string_view query, const Options& options) {
  std::string key = std::string(query) + "|" + std::to_string(options.max_results) + "|" +
                    (options.include_raw_content ? "1" : "0") + "|";
  for (const auto& e : options.engines) {
    key += e;
    key += ',';
  }
  return key;
}

void enrich_raw_content(std::vector<Result>& results, int timeout_ms, int concurrency) {
  if (results.empty()) return;
  concurrency = std::max(1, concurrency);
  std::mutex mu;
  size_t next = 0;

  auto worker = [&]() {
    for (;;) {
      size_t i = 0;
      {
        std::lock_guard lock(mu);
        if (next >= results.size()) return;
        i = next++;
      }
      auto body = extract(results[i].url, timeout_ms);
      if (!body.empty()) {
        results[i].raw_content = std::move(body);
      }
    }
  };

  std::vector<std::thread> threads;
  int n = std::min(concurrency, static_cast<int>(results.size()));
  threads.reserve(static_cast<size_t>(n));
  for (int t = 0; t < n; ++t) threads.emplace_back(worker);
  for (auto& th : threads) th.join();
}

}  // namespace

Response search(std::string_view query, const Options& options) {
  Response response;
  response.query = std::string(query);
  if (query.empty()) return response;

  const std::string key = cache_key(query, options);
  {
    std::lock_guard lock(g_cache_mu);
    auto it = g_cache.find(key);
    if (it != g_cache.end() && it->second.expires > std::chrono::steady_clock::now()) {
      return it->second.response;
    }
  }

  detail::EngineContext ctx;
  ctx.timeout_ms = options.timeout_ms;

  std::vector<std::future<std::vector<detail::SerpHit>>> futures;
  futures.reserve(options.engines.size());

  for (const auto& eng_name : options.engines) {
    futures.push_back(std::async(std::launch::async, [&, eng_name]() {
      auto eng = detail::make_engine(eng_name);
      if (!eng) return std::vector<detail::SerpHit>{};
      try {
        return eng->search(query, ctx);
      } catch (...) {
        return std::vector<detail::SerpHit>{};
      }
    }));
  }

  std::vector<std::vector<detail::SerpHit>> per_engine;
  per_engine.reserve(futures.size());
  for (auto& f : futures) {
    try {
      per_engine.push_back(f.get());
    } catch (...) {
      per_engine.emplace_back();
    }
  }

  auto ranked = detail::merge_and_rank(per_engine, options.max_results);
  response.results.reserve(ranked.size());
  for (auto& r : ranked) {
    Result out;
    out.title = std::move(r.hit.title);
    out.url = std::move(r.hit.url);
    out.content = std::move(r.hit.snippet);
    out.score = r.score;
    response.results.push_back(std::move(out));
  }

  if (options.include_raw_content) {
    enrich_raw_content(response.results, options.timeout_ms, options.fetch_concurrency);
  }

  {
    std::lock_guard lock(g_cache_mu);
    g_cache[key] = CacheEntry{response, std::chrono::steady_clock::now() + kCacheTtl};
    if (g_cache.size() > 256) {
      auto now = std::chrono::steady_clock::now();
      for (auto it = g_cache.begin(); it != g_cache.end();) {
        if (it->second.expires <= now)
          it = g_cache.erase(it);
        else
          ++it;
      }
    }
  }

  return response;
}

nlohmann::json to_json(const Response& response) {
  nlohmann::json results = nlohmann::json::array();
  for (const auto& r : response.results) {
    nlohmann::json item = {
        {"title", r.title},
        {"url", r.url},
        {"content", r.content},
        {"score", r.score},
    };
    if (r.raw_content) item["raw_content"] = *r.raw_content;
    results.push_back(std::move(item));
  }
  return nlohmann::json{
      {"query", response.query},
      {"results", std::move(results)},
  };
}

}  // namespace wsf

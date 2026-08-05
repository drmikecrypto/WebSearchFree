#include "wsf/wsf.hpp"

#include "engines/engine.hpp"
#include "http_client.hpp"
#include "html_utils.hpp"
#include "rank.hpp"

#include <chrono>
#include <cstdlib>
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

std::string resolve_searx_url(const Options& options) {
  if (!options.searx_url.empty()) return options.searx_url;
  if (const char* env = std::getenv("WSF_SEARX_URL")) {
    if (env[0] != '\0') return std::string(env);
  }
  return {};
}

std::string cache_key(std::string_view query, const Options& options) {
  std::string key = std::string(query) + "|" + std::to_string(options.max_results) + "|" +
                    (options.include_raw_content ? "1" : "0") + "|" + resolve_searx_url(options) +
                    "|";
  for (const auto& e : options.engines) {
    key += e;
    key += ',';
  }
  return key;
}

void enrich_raw_content(std::vector<Result>& results, int timeout_ms, int concurrency) {
  if (results.empty()) return;
  std::vector<std::string> urls;
  urls.reserve(results.size());
  for (const auto& r : results) urls.push_back(r.url);
  auto extracted = extract_many(urls, timeout_ms, concurrency);
  for (size_t i = 0; i < results.size() && i < extracted.size(); ++i) {
    if (extracted[i].status == "ok" && !extracted[i].raw_content.empty()) {
      results[i].raw_content = std::move(extracted[i].raw_content);
    }
  }
}

struct NamedOutcome {
  std::string name;
  detail::EngineOutcome outcome;
};

}  // namespace

Response search(std::string_view query, const Options& options) {
  const auto t0 = std::chrono::steady_clock::now();
  Response response;
  response.query = std::string(query);
  if (query.empty()) {
    response.warnings.push_back("empty query");
    return response;
  }

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
  ctx.searx_url = resolve_searx_url(options);

  std::vector<std::future<NamedOutcome>> futures;
  futures.reserve(options.engines.size());

  for (const auto& eng_name : options.engines) {
    futures.push_back(std::async(std::launch::async, [&, eng_name]() {
      NamedOutcome named;
      named.name = eng_name;
      auto eng = detail::make_engine(eng_name);
      if (!eng) {
        named.outcome.error = "unknown engine";
        return named;
      }
      if ((eng_name == "searx" || eng_name == "searxng") && ctx.searx_url.empty()) {
        named.outcome.error = "searx requires searx_url or WSF_SEARX_URL";
        return named;
      }
      try {
        named.outcome = eng->search(query, ctx);
      } catch (const std::exception& ex) {
        named.outcome.error = ex.what();
      } catch (...) {
        named.outcome.error = "engine threw unknown exception";
      }
      return named;
    }));
  }

  std::vector<std::vector<detail::SerpHit>> per_engine;
  per_engine.reserve(futures.size());
  for (auto& f : futures) {
    NamedOutcome named;
    try {
      named = f.get();
    } catch (const std::exception& ex) {
      named.name = "unknown";
      named.outcome.error = ex.what();
    } catch (...) {
      named.name = "unknown";
      named.outcome.error = "engine future failed";
    }

    EngineStatus st;
    st.name = named.name;
    st.hit_count = static_cast<int>(named.outcome.hits.size());
    if (!named.outcome.error.empty()) {
      st.ok = false;
      st.error = named.outcome.error;
      response.warnings.push_back(named.name + ": " + named.outcome.error);
    } else if (named.outcome.hits.empty()) {
      st.ok = false;
      st.error = "no results";
      response.warnings.push_back(named.name + ": no results");
    } else {
      st.ok = true;
    }
    response.engines.push_back(std::move(st));
    per_engine.push_back(std::move(named.outcome.hits));
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

  if (response.results.empty() && !options.engines.empty()) {
    response.warnings.push_back("all engines returned no usable results");
  }

  if (options.include_raw_content) {
    enrich_raw_content(response.results, options.timeout_ms, options.fetch_concurrency);
  }

  const auto t1 = std::chrono::steady_clock::now();
  response.response_time =
      std::chrono::duration<double>(t1 - t0).count();

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

  nlohmann::json engines = nlohmann::json::array();
  for (const auto& e : response.engines) {
    engines.push_back(nlohmann::json{
        {"name", e.name},
        {"ok", e.ok},
        {"hit_count", e.hit_count},
        {"error", e.error.empty() ? nullptr : nlohmann::json(e.error)},
    });
  }

  return nlohmann::json{
      {"query", response.query},
      {"results", std::move(results)},
      {"engines", std::move(engines)},
      {"warnings", response.warnings},
      {"response_time", response.response_time},
      {"answer", nullptr},
      {"images", nlohmann::json::array()},
      {"follow_up_questions", nullptr},
  };
}

std::vector<std::string> available_engines() { return detail::known_engine_names(); }

}  // namespace wsf

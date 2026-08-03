#include "engines/engine.hpp"
#include "http_client.hpp"
#include "html_utils.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace wsf::detail {
namespace {

class DdgEngine : public SearchEngine {
 public:
  std::string name() const override { return "ddg"; }

  std::vector<SerpHit> search(std::string_view query, const EngineContext& ctx) override {
    // Prefer GET on lite HTML endpoint (keyless, SSR).
    std::string url = "https://html.duckduckgo.com/html/?q=" + url_encode(query);
    auto resp = http_get(url, ctx.timeout_ms);
    if (!resp.error.empty() || resp.status >= 400 || resp.body.empty()) {
      // Fallback POST
      std::string form = "q=" + url_encode(query) + "&b=";
      resp = http_post_form("https://html.duckduckgo.com/html/", form, ctx.timeout_ms);
    }
    if (!resp.error.empty() || resp.body.empty()) return {};
    return parse_ddg_html(resp.body);
  }
};

class BraveEngine : public SearchEngine {
 public:
  std::string name() const override { return "brave"; }

  std::vector<SerpHit> search(std::string_view query, const EngineContext& ctx) override {
    std::string url = "https://search.brave.com/search?q=" + url_encode(query) + "&source=web";
    auto resp = http_get(url, ctx.timeout_ms);
    if (!resp.error.empty() || resp.body.empty()) return {};
    return parse_brave_html(resp.body);
  }
};

class WikipediaEngine : public SearchEngine {
 public:
  std::string name() const override { return "wikipedia"; }

  std::vector<SerpHit> search(std::string_view query, const EngineContext& ctx) override {
    // MediaWiki opensearch API — free, no key
    std::string url =
        "https://en.wikipedia.org/w/api.php?action=opensearch&limit=8&namespace=0&format=json&search=" +
        url_encode(query);
    auto resp = http_get(url, ctx.timeout_ms);
    if (!resp.error.empty() || resp.body.empty() || resp.status >= 400) return {};

    std::vector<SerpHit> hits;
    try {
      auto j = nlohmann::json::parse(resp.body);
      // ["query", [titles], [descs], [urls]]
      if (!j.is_array() || j.size() < 4) return {};
      const auto& titles = j[1];
      const auto& descs = j[2];
      const auto& urls = j[3];
      size_t n = std::min(titles.size(), urls.size());
      for (size_t i = 0; i < n; ++i) {
        SerpHit hit;
        hit.title = titles[i].get<std::string>();
        hit.url = normalize_url(urls[i].get<std::string>());
        if (i < descs.size() && descs[i].is_string()) hit.snippet = descs[i].get<std::string>();
        hit.rank = static_cast<int>(i);
        hit.engine = "wikipedia";
        hits.push_back(std::move(hit));
      }
    } catch (...) {
      return {};
    }
    return hits;
  }
};

}  // namespace

std::unique_ptr<SearchEngine> make_engine(std::string_view name) {
  if (name == "ddg" || name == "duckduckgo") return std::make_unique<DdgEngine>();
  if (name == "brave") return std::make_unique<BraveEngine>();
  if (name == "wikipedia" || name == "wiki") return std::make_unique<WikipediaEngine>();
  return nullptr;
}

}  // namespace wsf::detail

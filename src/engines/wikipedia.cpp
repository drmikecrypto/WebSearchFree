#include "engines/engine.hpp"
#include "http_client.hpp"
#include "html_utils.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace wsf::detail {
namespace {

class WikipediaEngine : public SearchEngine {
 public:
  std::string name() const override { return "wikipedia"; }

  EngineOutcome search(std::string_view query, const EngineContext& ctx) override {
    EngineOutcome out;
    std::string url =
        "https://en.wikipedia.org/w/api.php?action=opensearch&limit=8&namespace=0&format=json&search=" +
        url_encode(query);
    auto resp = http_get(url, ctx.timeout_ms);
    if (!resp.error.empty()) {
      out.error = resp.error;
      return out;
    }
    if (resp.body.empty() || resp.status >= 400) {
      out.error = resp.body.empty() ? "empty response body"
                                    : ("HTTP status " + std::to_string(resp.status));
      return out;
    }

    try {
      auto j = nlohmann::json::parse(resp.body);
      if (!j.is_array() || j.size() < 4) {
        out.error = "unexpected opensearch payload";
        return out;
      }
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
        out.hits.push_back(std::move(hit));
      }
    } catch (const std::exception& ex) {
      out.error = ex.what();
      return out;
    }
    if (out.hits.empty()) out.error = "no wikipedia matches";
    return out;
  }
};

}  // namespace

std::unique_ptr<SearchEngine> make_wikipedia_engine() {
  return std::make_unique<WikipediaEngine>();
}

}  // namespace wsf::detail

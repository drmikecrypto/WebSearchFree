#include "engines/engine.hpp"
#include "http_client.hpp"
#include "html_utils.hpp"

namespace wsf::detail {
namespace {

std::string trim_trailing_slash(std::string s) {
  while (!s.empty() && s.back() == '/') s.pop_back();
  return s;
}

class SearxEngine : public SearchEngine {
 public:
  std::string name() const override { return "searx"; }

  EngineOutcome search(std::string_view query, const EngineContext& ctx) override {
    EngineOutcome out;
    if (ctx.searx_url.empty()) {
      out.error = "searx_url not configured";
      return out;
    }
    std::string base = trim_trailing_slash(ctx.searx_url);
    std::string url = base + "/search?q=" + url_encode(query) + "&format=json";
    auto resp = http_get(url, ctx.timeout_ms);
    if (!resp.error.empty()) {
      out.error = resp.error;
      return out;
    }
    if (resp.status >= 400) {
      out.error = "HTTP status " + std::to_string(resp.status);
      return out;
    }
    if (resp.body.empty()) {
      out.error = "empty response body";
      return out;
    }
    out.hits = parse_searx_json(resp.body);
    if (out.hits.empty()) out.error = "failed to parse SearXNG JSON";
    return out;
  }
};

}  // namespace

std::unique_ptr<SearchEngine> make_searx_engine() { return std::make_unique<SearxEngine>(); }

}  // namespace wsf::detail

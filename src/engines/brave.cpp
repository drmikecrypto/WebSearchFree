#include "engines/engine.hpp"
#include "http_client.hpp"
#include "html_utils.hpp"

namespace wsf::detail {
namespace {

class BraveEngine : public SearchEngine {
 public:
  std::string name() const override { return "brave"; }

  EngineOutcome search(std::string_view query, const EngineContext& ctx) override {
    EngineOutcome out;
    std::string url = "https://search.brave.com/search?q=" + url_encode(query) + "&source=web";
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
    out.hits = parse_brave_html(resp.body);
    if (out.hits.empty()) out.error = "failed to parse SERP HTML";
    return out;
  }
};

}  // namespace

std::unique_ptr<SearchEngine> make_brave_engine() { return std::make_unique<BraveEngine>(); }

}  // namespace wsf::detail

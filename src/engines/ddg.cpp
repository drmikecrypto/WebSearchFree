#include "engines/engine.hpp"
#include "http_client.hpp"
#include "html_utils.hpp"

namespace wsf::detail {
namespace {

class DdgEngine : public SearchEngine {
 public:
  std::string name() const override { return "ddg"; }

  EngineOutcome search(std::string_view query, const EngineContext& ctx) override {
    EngineOutcome out;
    std::string url = "https://html.duckduckgo.com/html/?q=" + url_encode(query);
    auto resp = http_get(url, ctx.timeout_ms);
    if (!resp.error.empty() || resp.status >= 400 || resp.body.empty()) {
      std::string form = "q=" + url_encode(query) + "&b=";
      resp = http_post_form("https://html.duckduckgo.com/html/", form, ctx.timeout_ms);
    }
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
    out.hits = parse_ddg_html(resp.body);
    if (out.hits.empty()) out.error = "failed to parse SERP HTML";
    return out;
  }
};

}  // namespace

std::unique_ptr<SearchEngine> make_ddg_engine() { return std::make_unique<DdgEngine>(); }

}  // namespace wsf::detail

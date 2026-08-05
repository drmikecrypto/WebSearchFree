#include "engines/engine.hpp"

#include <memory>

namespace wsf::detail {

std::unique_ptr<SearchEngine> make_ddg_engine();
std::unique_ptr<SearchEngine> make_brave_engine();
std::unique_ptr<SearchEngine> make_wikipedia_engine();
std::unique_ptr<SearchEngine> make_searx_engine();

std::unique_ptr<SearchEngine> make_engine(std::string_view name) {
  if (name == "ddg" || name == "duckduckgo") return make_ddg_engine();
  if (name == "brave") return make_brave_engine();
  if (name == "wikipedia" || name == "wiki") return make_wikipedia_engine();
  if (name == "searx" || name == "searxng") return make_searx_engine();
  return nullptr;
}

std::vector<std::string> known_engine_names() {
  return {"ddg", "brave", "wikipedia", "searx"};
}

}  // namespace wsf::detail

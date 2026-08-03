#pragma once

#include "html_utils.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace wsf::detail {

struct EngineContext {
  int timeout_ms = 8000;
};

class SearchEngine {
 public:
  virtual ~SearchEngine() = default;
  virtual std::string name() const = 0;
  virtual std::vector<SerpHit> search(std::string_view query, const EngineContext& ctx) = 0;
};

std::unique_ptr<SearchEngine> make_engine(std::string_view name);

}  // namespace wsf::detail

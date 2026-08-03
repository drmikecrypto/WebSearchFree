#pragma once

#include "html_utils.hpp"

#include <string>
#include <vector>

namespace wsf::detail {

struct RankedHit {
  SerpHit hit;
  double score = 0.0;
};

/// Merge hits from multiple engines: dedupe by normalized URL, boost overlaps.
std::vector<RankedHit> merge_and_rank(const std::vector<std::vector<SerpHit>>& per_engine,
                                      int max_results);

}  // namespace wsf::detail

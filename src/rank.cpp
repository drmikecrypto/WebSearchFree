#include "rank.hpp"

#include <algorithm>
#include <unordered_map>

namespace wsf::detail {

std::vector<RankedHit> merge_and_rank(const std::vector<std::vector<SerpHit>>& per_engine,
                                      int max_results) {
  struct Acc {
    SerpHit hit;
    double score = 0.0;
    int engines = 0;
  };
  std::unordered_map<std::string, Acc> by_url;
  by_url.reserve(64);

  for (const auto& hits : per_engine) {
    for (const auto& h : hits) {
      std::string key = normalize_url(h.url);
      if (key.empty()) continue;
      auto& acc = by_url[key];
      if (acc.hit.url.empty()) {
        acc.hit = h;
        acc.hit.url = key;
      } else {
        if (acc.hit.snippet.size() < h.snippet.size()) acc.hit.snippet = h.snippet;
        if (acc.hit.title.empty()) acc.hit.title = h.title;
      }
      // Higher rank (lower index) scores more; multi-engine agreement boosts.
      double rank_score = 1.0 / (1.0 + static_cast<double>(h.rank));
      acc.score += rank_score;
      acc.engines += 1;
    }
  }

  std::vector<RankedHit> ranked;
  ranked.reserve(by_url.size());
  for (auto& [_, acc] : by_url) {
    RankedHit r;
    r.hit = std::move(acc.hit);
    r.score = acc.score * (1.0 + 0.35 * static_cast<double>(acc.engines - 1));
    ranked.push_back(std::move(r));
  }

  std::sort(ranked.begin(), ranked.end(),
            [](const RankedHit& a, const RankedHit& b) { return a.score > b.score; });

  if (max_results > 0 && static_cast<int>(ranked.size()) > max_results) {
    ranked.resize(static_cast<size_t>(max_results));
  }
  return ranked;
}

}  // namespace wsf::detail

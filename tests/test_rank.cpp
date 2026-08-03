#include "../src/html_utils.hpp"
#include "../src/rank.hpp"

#include <iostream>

static int g_failed = 0;
#define CHECK(cond)                                                                            \
  do {                                                                                         \
    if (!(cond)) {                                                                             \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond << "\n";             \
      ++g_failed;                                                                              \
    }                                                                                          \
  } while (0)

void test_rank_merge() {
  using wsf::detail::SerpHit;
  SerpHit a{"Alpha", "https://example.com/a", "snip a", 0, "ddg"};
  SerpHit b{"Alpha alt", "https://example.com/a", "longer snippet here", 1, "brave"};
  SerpHit c{"Beta", "https://example.com/b", "snip b", 0, "ddg"};

  auto ranked = wsf::detail::merge_and_rank({{a, c}, {b}}, 10);
  CHECK(ranked.size() == 2);
  // Overlap on example.com/a should rank first
  CHECK(ranked[0].hit.url.find("/a") != std::string::npos);
  CHECK(ranked[0].hit.snippet.find("longer") != std::string::npos);
  CHECK(ranked[0].score > ranked[1].score);
}

int test_rank_main() {
  test_rank_merge();
  return g_failed;
}

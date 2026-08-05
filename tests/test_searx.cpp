#include "../src/html_utils.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifndef WSF_FIXTURES_DIR
#  define WSF_FIXTURES_DIR "tests/fixtures"
#endif

static int g_failed = 0;
#define CHECK(cond)                                                                            \
  do {                                                                                         \
    if (!(cond)) {                                                                             \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond << "\n";             \
      ++g_failed;                                                                              \
    }                                                                                          \
  } while (0)

static std::string read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void test_searx_parse() {
  auto body = read_file(std::string(WSF_FIXTURES_DIR) + "/searx_sample.json");
  auto hits = wsf::detail::parse_searx_json(body);
  CHECK(hits.size() == 2);
  CHECK(hits[0].title == "SearXNG Sample");
  CHECK(hits[0].url.find("utm_campaign") == std::string::npos);
  CHECK(hits[0].url.find("id=1") != std::string::npos);
  CHECK(hits[0].snippet.find("Sample SearXNG") != std::string::npos);
  CHECK(hits[1].snippet.find("Alt snippet") != std::string::npos);
  CHECK(hits[0].engine == "searx");
}

int test_searx_main() {
  test_searx_parse();
  return g_failed;
}

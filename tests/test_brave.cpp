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

void test_brave_parse() {
  auto html = read_file(std::string(WSF_FIXTURES_DIR) + "/brave_sample.html");
  auto hits = wsf::detail::parse_brave_html(html);
  CHECK(hits.size() >= 2);
  CHECK(hits[0].title.find("Brave Sample") != std::string::npos);
  CHECK(hits[0].url.find("example.com/brave-result") != std::string::npos);
  CHECK(hits[0].url.find("utm_source") == std::string::npos);
  CHECK(hits[0].engine == "brave");
}

int test_brave_main() {
  test_brave_parse();
  return g_failed;
}

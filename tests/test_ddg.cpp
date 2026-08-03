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

void test_ddg_parse() {
  auto html = read_file(std::string(WSF_FIXTURES_DIR) + "/ddg_sample.html");
  CHECK(!html.empty());
  auto hits = wsf::detail::parse_ddg_html(html);
  CHECK(hits.size() == 3);
  CHECK(hits[0].title.find("Metasearch") != std::string::npos);
  CHECK(hits[0].url == "https://en.wikipedia.org/wiki/Metasearch_engine");
  CHECK(hits[1].url == "https://github.com/searxng/searxng");
  CHECK(hits[2].url.find("example.com") != std::string::npos);
  CHECK(hits[0].engine == "ddg");
}

int test_ddg_main() {
  test_ddg_parse();
  return g_failed;
}

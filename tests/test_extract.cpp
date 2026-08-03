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

void test_extract() {
  auto html = read_file(std::string(WSF_FIXTURES_DIR) + "/article_sample.html");
  auto text = wsf::detail::extract_main_content(html);
  CHECK(text.find("main article content") != std::string::npos);
  CHECK(text.find("var x") == std::string::npos);
}

int test_extract_main() {
  test_extract();
  return g_failed;
}

// Single test binary entry
int main() {
  extern int test_ddg_main();
  extern int test_html_utils_main();
  extern int test_rank_main();
  int failed = 0;
  failed += test_ddg_main();
  failed += test_html_utils_main();
  failed += test_rank_main();
  failed += test_extract_main();
  if (failed == 0) {
    std::cout << "All tests passed\n";
    return 0;
  }
  std::cerr << failed << " assertion(s) failed\n";
  return 1;
}

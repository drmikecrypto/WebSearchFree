#include "../src/html_utils.hpp"

#include <iostream>
#include <string>

static int g_failed = 0;
#define CHECK(cond)                                                                            \
  do {                                                                                         \
    if (!(cond)) {                                                                             \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond << "\n";             \
      ++g_failed;                                                                              \
    }                                                                                          \
  } while (0)

void test_html_utils() {
  CHECK(wsf::detail::html_unescape("a &amp; b") == "a & b");
  CHECK(wsf::detail::url_encode("a b") == "a+b");
  CHECK(wsf::detail::normalize_url("HTTPS://Example.COM/Path#frag") ==
        "https://example.com/Path");
  auto text = wsf::detail::collapse_whitespace(wsf::detail::strip_tags("<p>Hi <b>there</b></p>"));
  CHECK(text.find("Hi") != std::string::npos);
  CHECK(text.find("there") != std::string::npos);
}

// Linked from test runner
int test_html_utils_main() {
  test_html_utils();
  return g_failed;
}

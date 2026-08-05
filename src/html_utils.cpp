#include "html_utils.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>

namespace wsf::detail {
namespace {

bool iequals(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

size_t find_ci(std::string_view hay, std::string_view needle, size_t pos = 0) {
  if (needle.empty() || pos >= hay.size()) return std::string_view::npos;
  auto it = std::search(hay.begin() + static_cast<std::ptrdiff_t>(pos), hay.end(), needle.begin(),
                        needle.end(), [](char a, char b) {
                          return std::tolower(static_cast<unsigned char>(a)) ==
                                 std::tolower(static_cast<unsigned char>(b));
                        });
  if (it == hay.end()) return std::string_view::npos;
  return static_cast<size_t>(it - hay.begin());
}

std::string decode_ddg_redirect(std::string url) {
  // DuckDuckGo wraps links as //duckduckgo.com/l/?uddg=<encoded>&...
  auto pos = url.find("uddg=");
  if (pos == std::string::npos) return url;
  pos += 5;
  auto end = url.find('&', pos);
  std::string enc = url.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
  // percent-decode
  std::string out;
  out.reserve(enc.size());
  for (size_t i = 0; i < enc.size(); ++i) {
    if (enc[i] == '%' && i + 2 < enc.size()) {
      auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
      };
      int hi = hex(enc[i + 1]);
      int lo = hex(enc[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    if (enc[i] == '+')
      out.push_back(' ');
    else
      out.push_back(enc[i]);
  }
  return out;
}

}  // namespace

std::string html_unescape(std::string_view input) {
  static const std::unordered_map<std::string_view, char> ents = {
      {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''}, {"nbsp", ' '},
  };
  std::string out;
  out.reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '&') {
      auto semi = input.find(';', i + 1);
      if (semi != std::string_view::npos && semi - i < 10) {
        auto name = input.substr(i + 1, semi - i - 1);
        if (!name.empty() && name[0] == '#') {
          int code = 0;
          if (name.size() > 1 && (name[1] == 'x' || name[1] == 'X')) {
            for (size_t j = 2; j < name.size(); ++j) {
              char c = name[j];
              code *= 16;
              if (c >= '0' && c <= '9')
                code += c - '0';
              else if (c >= 'a' && c <= 'f')
                code += c - 'a' + 10;
              else if (c >= 'A' && c <= 'F')
                code += c - 'A' + 10;
            }
          } else {
            for (size_t j = 1; j < name.size(); ++j) {
              if (name[j] < '0' || name[j] > '9') {
                code = -1;
                break;
              }
              code = code * 10 + (name[j] - '0');
            }
          }
          if (code >= 0 && code < 128) {
            out.push_back(static_cast<char>(code));
            i = semi;
            continue;
          }
        } else {
          auto it = ents.find(name);
          if (it != ents.end()) {
            out.push_back(it->second);
            i = semi;
            continue;
          }
        }
      }
    }
    out.push_back(input[i]);
  }
  return out;
}

std::string strip_tags(std::string_view html) {
  std::string out;
  out.reserve(html.size());
  bool in_tag = false;
  bool in_script = false;
  bool in_style = false;
  for (size_t i = 0; i < html.size(); ++i) {
    if (!in_tag && html[i] == '<') {
      auto close = html.find('>', i);
      if (close == std::string_view::npos) break;
      auto tag = html.substr(i + 1, close - i - 1);
      // detect script/style
      size_t start = 0;
      if (!tag.empty() && tag[0] == '/') ++start;
      while (start < tag.size() && std::isspace(static_cast<unsigned char>(tag[start]))) ++start;
      auto name_end = start;
      while (name_end < tag.size() && std::isalnum(static_cast<unsigned char>(tag[name_end])))
        ++name_end;
      auto name = tag.substr(start, name_end - start);
      bool closing = !tag.empty() && tag[0] == '/';
      if (iequals(name, "script")) in_script = !closing;
      if (iequals(name, "style")) in_style = !closing;
      if (iequals(name, "br") || iequals(name, "p") || iequals(name, "div") || iequals(name, "li") ||
          iequals(name, "tr") || iequals(name, "h1") || iequals(name, "h2") || iequals(name, "h3")) {
        if (!closing) out.push_back('\n');
      }
      in_tag = false;
      i = close;
      continue;
    }
    if (in_script || in_style) continue;
    out.push_back(html[i]);
  }
  return html_unescape(out);
}

std::string collapse_whitespace(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  bool prev_space = true;
  bool prev_nl = false;
  for (char c : input) {
    if (c == '\r') continue;
    if (c == '\n') {
      if (!prev_nl && !out.empty()) out.push_back('\n');
      prev_nl = true;
      prev_space = true;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!prev_space) out.push_back(' ');
      prev_space = true;
      prev_nl = false;
      continue;
    }
    out.push_back(c);
    prev_space = false;
    prev_nl = false;
  }
  while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) out.pop_back();
  return out;
}

namespace {

bool is_tracking_param(std::string_view key) {
  static const char* kTracking[] = {
      "utm_source", "utm_medium", "utm_campaign", "utm_term", "utm_content", "utm_id",
      "fbclid",     "gclid",      "gbraid",      "wbraid",   "mc_cid",     "mc_eid",
      "msclkid",    "yclid",      "_ga",         "ref",
  };
  std::string lower(key);
  for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (const char* t : kTracking) {
    if (lower == t) return true;
  }
  return false;
}

std::string strip_tracking_params(std::string u) {
  auto q = u.find('?');
  if (q == std::string::npos) return u;
  std::string base = u.substr(0, q);
  std::string query = u.substr(q + 1);
  std::string kept;
  size_t start = 0;
  while (start < query.size()) {
    size_t amp = query.find('&', start);
    std::string pair =
        amp == std::string::npos ? query.substr(start) : query.substr(start, amp - start);
    if (!pair.empty()) {
      auto eq = pair.find('=');
      std::string key = eq == std::string::npos ? pair : pair.substr(0, eq);
      if (!is_tracking_param(key)) {
        if (!kept.empty()) kept.push_back('&');
        kept += pair;
      }
    }
    if (amp == std::string::npos) break;
    start = amp + 1;
  }
  if (kept.empty()) return base;
  return base + "?" + kept;
}

}  // namespace

std::string normalize_url(std::string_view url) {
  std::string u(url);
  u = html_unescape(u);
  if (u.rfind("//", 0) == 0) u = "https:" + u;
  // strip fragment
  auto hash = u.find('#');
  if (hash != std::string::npos) u.resize(hash);
  // lowercase scheme+host
  auto scheme = u.find("://");
  if (scheme != std::string::npos) {
    auto host_end = u.find('/', scheme + 3);
    size_t end = host_end == std::string::npos ? u.size() : host_end;
    for (size_t i = 0; i < end; ++i) u[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(u[i])));
    // strip trailing slash on bare host
    if (host_end != std::string::npos && host_end == u.size() - 1) u.pop_back();
  }
  return strip_tracking_params(std::move(u));
}

std::string url_encode(std::string_view value) {
  static const char* hex = "0123456789ABCDEF";
  std::string out;
  out.reserve(value.size() * 3);
  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else if (c == ' ') {
      out.push_back('+');
    } else {
      out.push_back('%');
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 0xF]);
    }
  }
  return out;
}

std::string attribute_value(std::string_view tag, std::string_view attr) {
  auto pos = find_ci(tag, attr);
  if (pos == std::string_view::npos) return {};
  pos += attr.size();
  while (pos < tag.size() && std::isspace(static_cast<unsigned char>(tag[pos]))) ++pos;
  if (pos >= tag.size() || tag[pos] != '=') return {};
  ++pos;
  while (pos < tag.size() && std::isspace(static_cast<unsigned char>(tag[pos]))) ++pos;
  if (pos >= tag.size()) return {};
  char quote = 0;
  if (tag[pos] == '"' || tag[pos] == '\'') {
    quote = tag[pos++];
    auto end = tag.find(quote, pos);
    if (end == std::string_view::npos) return {};
    return html_unescape(tag.substr(pos, end - pos));
  }
  size_t end = pos;
  while (end < tag.size() && !std::isspace(static_cast<unsigned char>(tag[end])) && tag[end] != '>')
    ++end;
  return html_unescape(tag.substr(pos, end - pos));
}

std::vector<std::string> find_tag_blocks(std::string_view html, std::string_view open_needle) {
  std::vector<std::string> blocks;
  size_t pos = 0;
  while ((pos = find_ci(html, open_needle, pos)) != std::string_view::npos) {
    auto tag_end = html.find('>', pos);
    if (tag_end == std::string_view::npos) break;
    // find matching close approximately by next sibling open or end
    size_t content_start = tag_end + 1;
    // take until next open_needle or a reasonable chunk
    size_t next = find_ci(html, open_needle, content_start);
    size_t content_end = next == std::string_view::npos ? html.size() : next;
    // also try to stop at a clear closing pattern for div
    blocks.emplace_back(html.substr(pos, content_end - pos));
    pos = content_start;
  }
  return blocks;
}

std::string extract_main_content(std::string_view html) {
  // Prefer <article> then <main> then densest <div>/<p> region.
  auto try_region = [&](std::string_view open_tag, std::string_view close_tag) -> std::string {
    size_t start = find_ci(html, open_tag);
    if (start == std::string_view::npos) return {};
    auto gt = html.find('>', start);
    if (gt == std::string_view::npos) return {};
    size_t end = find_ci(html, close_tag, gt);
    if (end == std::string_view::npos) end = std::min(html.size(), gt + 200000);
    auto text = collapse_whitespace(strip_tags(html.substr(gt + 1, end - gt - 1)));
    return text;
  };

  std::string best = try_region("<article", "</article>");
  if (best.size() < 200) {
    auto main = try_region("<main", "</main>");
    if (main.size() > best.size()) best = std::move(main);
  }
  if (best.size() < 200) {
    // Fall back: strip whole body
    size_t body = find_ci(html, "<body");
    if (body != std::string_view::npos) {
      auto gt = html.find('>', body);
      size_t end = find_ci(html, "</body>", gt);
      if (gt != std::string_view::npos) {
        best = collapse_whitespace(strip_tags(
            html.substr(gt + 1, end == std::string_view::npos ? html.size() - gt - 1 : end - gt - 1)));
      }
    } else {
      best = collapse_whitespace(strip_tags(html));
    }
  }
  // Cap extremely long pages for LLM friendliness
  constexpr size_t kMax = 50000;
  if (best.size() > kMax) best.resize(kMax);
  return best;
}

std::vector<SerpHit> parse_ddg_html(std::string_view html) {
  std::vector<SerpHit> hits;
  // Results look like: <div class="result ..."> ... <a class="result__a" href="...">title</a>
  //                    <a class="result__snippet" ...>snippet</a>
  size_t pos = 0;
  int rank = 0;
  while ((pos = find_ci(html, "result__a", pos)) != std::string_view::npos) {
    // walk back to opening <a
    size_t a_start = html.rfind('<', pos);
    if (a_start == std::string_view::npos) {
      ++pos;
      continue;
    }
    auto a_gt = html.find('>', a_start);
    if (a_gt == std::string_view::npos) break;
    auto a_tag = html.substr(a_start, a_gt - a_start + 1);
    if (find_ci(a_tag, "result__a") == std::string_view::npos) {
      pos = a_gt + 1;
      continue;
    }
    std::string href = attribute_value(a_tag, "href");
    auto a_close = find_ci(html, "</a>", a_gt);
    if (a_close == std::string_view::npos) break;
    std::string title = collapse_whitespace(strip_tags(html.substr(a_gt + 1, a_close - a_gt - 1)));

    std::string snippet;
    size_t snip = find_ci(html, "result__snippet", a_close);
    size_t next_result = find_ci(html, "result__a", a_close + 1);
    if (snip != std::string_view::npos && (next_result == std::string_view::npos || snip < next_result)) {
      auto snip_gt = html.find('>', snip);
      auto snip_close = find_ci(html, "</a>", snip_gt);
      if (snip_close == std::string_view::npos) snip_close = find_ci(html, "</td>", snip_gt);
      if (snip_gt != std::string_view::npos && snip_close != std::string_view::npos) {
        snippet = collapse_whitespace(strip_tags(html.substr(snip_gt + 1, snip_close - snip_gt - 1)));
      }
    }

    href = decode_ddg_redirect(href);
    href = normalize_url(href);
    if (!href.empty() && !title.empty() && href.find("duckduckgo.com") == std::string::npos) {
      SerpHit hit;
      hit.title = std::move(title);
      hit.url = std::move(href);
      hit.snippet = std::move(snippet);
      hit.rank = rank++;
      hit.engine = "ddg";
      hits.push_back(std::move(hit));
    }
    pos = a_close + 4;
  }
  return hits;
}

std::vector<SerpHit> parse_brave_html(std::string_view html) {
  std::vector<SerpHit> hits;
  // Brave organic results often use anchors with class containing "title"
  // and descriptions with "snippet-description".
  size_t pos = 0;
  int rank = 0;
  while (true) {
    size_t t1 = find_ci(html, "class=\"title\"", pos);
    size_t t2 = find_ci(html, "class='title'", pos);
    size_t title_pos = std::string_view::npos;
    if (t1 == std::string_view::npos)
      title_pos = t2;
    else if (t2 == std::string_view::npos)
      title_pos = t1;
    else
      title_pos = std::min(t1, t2);
    if (title_pos == std::string_view::npos) break;

    size_t window_start = title_pos > 200 ? title_pos - 200 : 0;
    size_t rel = find_ci(html.substr(window_start, title_pos - window_start + 80), "<a");
    size_t a_start = (rel == std::string_view::npos) ? std::string_view::npos : window_start + rel;
    if (a_start == std::string_view::npos) {
      a_start = find_ci(html, "<a", title_pos);
    }
    if (a_start == std::string_view::npos) {
      pos = title_pos + 5;
      continue;
    }
    auto a_gt = html.find('>', a_start);
    if (a_gt == std::string_view::npos) break;
    auto a_tag = html.substr(a_start, a_gt - a_start + 1);
    std::string href = attribute_value(a_tag, "href");
    auto a_close = find_ci(html, "</a>", a_gt);
    if (a_close == std::string_view::npos) break;
    std::string title = collapse_whitespace(strip_tags(html.substr(a_gt + 1, a_close - a_gt - 1)));

    std::string snippet;
    size_t snip = find_ci(html, "snippet-description", a_close);
    size_t next_title = title_pos + 5;
    size_t n1 = find_ci(html, "class=\"title\"", next_title);
    size_t n2 = find_ci(html, "class='title'", next_title);
    size_t next = std::string_view::npos;
    if (n1 == std::string_view::npos)
      next = n2;
    else if (n2 == std::string_view::npos)
      next = n1;
    else
      next = std::min(n1, n2);

    if (snip != std::string_view::npos && (next == std::string_view::npos || snip < next)) {
      auto snip_gt = html.find('>', snip);
      auto snip_close = find_ci(html, "</p>", snip_gt);
      if (snip_close == std::string_view::npos) snip_close = find_ci(html, "</div>", snip_gt);
      if (snip_gt != std::string_view::npos && snip_close != std::string_view::npos) {
        snippet = collapse_whitespace(strip_tags(html.substr(snip_gt + 1, snip_close - snip_gt - 1)));
      }
    }

    href = normalize_url(href);
    if (!href.empty() && !title.empty() && href.rfind("http", 0) == 0) {
      SerpHit hit;
      hit.title = std::move(title);
      hit.url = std::move(href);
      hit.snippet = std::move(snippet);
      hit.rank = rank++;
      hit.engine = "brave";
      hits.push_back(std::move(hit));
    }
    pos = a_close + 4;
  }
  return hits;
}

std::vector<SerpHit> parse_searx_json(std::string_view json_body) {
  std::vector<SerpHit> hits;
  try {
    auto j = nlohmann::json::parse(json_body);
    if (!j.contains("results") || !j["results"].is_array()) return hits;
    int rank = 0;
    for (const auto& item : j["results"]) {
      SerpHit hit;
      hit.title = item.value("title", "");
      hit.url = normalize_url(item.value("url", ""));
      hit.snippet = item.value("content", item.value("snippet", ""));
      hit.rank = rank++;
      hit.engine = "searx";
      if (!hit.url.empty() && !hit.title.empty()) hits.push_back(std::move(hit));
    }
  } catch (...) {
    return {};
  }
  return hits;
}

}  // namespace wsf::detail

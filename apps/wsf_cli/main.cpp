#include "wsf/wsf.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#if defined(WSF_HAS_SERVER)
#  include "httplib.h"
#endif

namespace {

void print_usage() {
  std::cerr
      << "WebSearchFree (wsf) " << wsf::kVersion << " — free, keyless metasearch for AI\n\n"
      << "Usage:\n"
      << "  wsf search <query> [--max N] [--raw] [--json|--text] [--engines ddg,brave,wikipedia]\n"
      << "                      [--searx-url URL] [--timeout MS]\n"
      << "  wsf extract <url> [--json] [--timeout MS]\n"
#if defined(WSF_HAS_SERVER)
      << "  wsf serve [--host 127.0.0.1] [--port 8080] [--searx-url URL]\n"
#endif
      << "\nNo API keys required. Optional SearXNG: --searx-url or WSF_SEARX_URL.\n";
}

std::vector<std::string> split_csv(const std::string& s) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) out.push_back(item);
  }
  return out;
}

void warn_unknown_engines(const std::vector<std::string>& engines) {
  static const std::unordered_set<std::string> known = {
      "ddg", "duckduckgo", "brave", "wikipedia", "wiki", "searx", "searxng"};
  for (const auto& e : engines) {
    if (!known.count(e)) {
      std::cerr << "warning: unknown engine '" << e << "'\n";
    }
  }
}

std::string env_or_empty(const char* key) {
  if (const char* v = std::getenv(key)) return v;
  return {};
}

int cmd_search(std::vector<std::string> args) {
  if (args.empty()) {
    print_usage();
    return 2;
  }
  wsf::Options opt;
  opt.searx_url = env_or_empty("WSF_SEARX_URL");
  bool as_json = true;
  std::string query;
  for (size_t i = 0; i < args.size(); ++i) {
    const auto& a = args[i];
    if (a == "--max" && i + 1 < args.size()) {
      opt.max_results = std::stoi(args[++i]);
    } else if (a == "--raw") {
      opt.include_raw_content = true;
    } else if (a == "--json") {
      as_json = true;
    } else if (a == "--text") {
      as_json = false;
    } else if (a == "--engines" && i + 1 < args.size()) {
      opt.engines = split_csv(args[++i]);
    } else if (a == "--timeout" && i + 1 < args.size()) {
      opt.timeout_ms = std::stoi(args[++i]);
    } else if (a == "--searx-url" && i + 1 < args.size()) {
      opt.searx_url = args[++i];
    } else if (!a.empty() && a[0] == '-') {
      std::cerr << "Unknown flag: " << a << "\n";
      return 2;
    } else {
      if (!query.empty()) query.push_back(' ');
      query += a;
    }
  }
  if (query.empty()) {
    print_usage();
    return 2;
  }

  warn_unknown_engines(opt.engines);
  auto resp = wsf::search(query, opt);
  if (as_json) {
    std::cout << wsf::to_json(resp).dump(2) << "\n";
  } else {
    if (resp.results.empty()) {
      std::cout << "No results.\n";
      for (const auto& w : resp.warnings) std::cerr << "warning: " << w << "\n";
    } else {
      for (size_t i = 0; i < resp.results.size(); ++i) {
        const auto& r = resp.results[i];
        std::cout << (i + 1) << ". " << r.title << "\n   " << r.url << "\n   " << r.content
                  << "\n\n";
      }
    }
  }
  return 0;
}

int cmd_extract(std::vector<std::string> args) {
  bool as_json = false;
  int timeout_ms = 8000;
  std::string url;
  for (size_t i = 0; i < args.size(); ++i) {
    const auto& a = args[i];
    if (a == "--json")
      as_json = true;
    else if (a == "--timeout" && i + 1 < args.size())
      timeout_ms = std::stoi(args[++i]);
    else if (!a.empty() && a[0] != '-')
      url = a;
  }
  if (url.empty()) {
    print_usage();
    return 2;
  }
  auto result = wsf::extract_one(url, timeout_ms);
  if (as_json) {
    std::cout << wsf::to_json(result).dump(2) << "\n";
  } else {
    if (result.status != "ok") {
      std::cerr << "extract failed (" << result.status << "): " << result.error << "\n";
      return 1;
    }
    std::cout << result.raw_content << "\n";
  }
  return result.status == "ok" ? 0 : 1;
}

#if defined(WSF_HAS_SERVER)

void set_cors(httplib::Response& res) {
  res.set_header("Access-Control-Allow-Origin", "*");
  res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

const char* kUiHtml = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>WebSearchFree</title>
<style>
  :root { --bg:#0f1419; --panel:#1a2332; --text:#e7ecf3; --muted:#8b9bb4; --accent:#3d9cf0; --ok:#3ecf8e; --bad:#f07178; }
  * { box-sizing: border-box; }
  body { margin:0; font-family: "Segoe UI", system-ui, sans-serif; background: radial-gradient(1200px 600px at 10% -10%, #1b2a44, var(--bg)); color: var(--text); min-height:100vh; }
  main { max-width: 880px; margin: 0 auto; padding: 2.5rem 1.25rem 4rem; }
  h1 { font-size: 1.75rem; letter-spacing: -0.02em; margin: 0 0 .25rem; }
  .sub { color: var(--muted); margin-bottom: 1.5rem; }
  form { display:flex; gap:.5rem; flex-wrap:wrap; margin-bottom: 1rem; }
  input[type=search], input[type=url] { flex:1; min-width: 220px; background: var(--panel); border:1px solid #2c3b52; color:var(--text); border-radius:8px; padding:.75rem .9rem; font-size:1rem; }
  button { background: var(--accent); color:#041018; border:0; border-radius:8px; padding:.75rem 1.1rem; font-weight:600; cursor:pointer; }
  button:disabled { opacity:.6; cursor:wait; }
  .meta { color: var(--muted); font-size:.9rem; margin: .5rem 0 1rem; }
  .warn { color: #e6c07b; font-size:.85rem; margin:.25rem 0; }
  article { background: rgba(26,35,50,.85); border:1px solid #2c3b52; border-radius:10px; padding:1rem 1.1rem; margin-bottom:.75rem; }
  article h2 { font-size:1.05rem; margin:0 0 .35rem; }
  article a { color: var(--accent); word-break: break-all; font-size:.9rem; }
  article p { margin:.5rem 0 0; color:#c5d0e0; line-height:1.45; }
  .score { float:right; color: var(--muted); font-size:.8rem; }
  pre { white-space: pre-wrap; background: var(--panel); border-radius:8px; padding:1rem; border:1px solid #2c3b52; max-height:320px; overflow:auto; }
  .tabs { display:flex; gap:.5rem; margin-bottom:1rem; }
  .tabs button { background:#243247; color:var(--text); }
  .tabs button.active { background: var(--accent); color:#041018; }
  .hidden { display:none; }
  footer { margin-top:2rem; color:var(--muted); font-size:.8rem; }
</style>
</head>
<body>
<main>
  <h1>WebSearchFree</h1>
  <p class="sub">Keyless metasearch + extract — local Tavily-shaped API</p>
  <div class="tabs">
    <button type="button" id="tab-search" class="active">Search</button>
    <button type="button" id="tab-extract">Extract</button>
  </div>
  <section id="panel-search">
    <form id="search-form">
      <input type="search" id="q" placeholder="Search the web…" required autofocus/>
      <button type="submit" id="search-btn">Search</button>
    </form>
    <div class="meta" id="search-meta"></div>
    <div id="warnings"></div>
    <div id="results"></div>
  </section>
  <section id="panel-extract" class="hidden">
    <form id="extract-form">
      <input type="url" id="url" placeholder="https://example.com" required/>
      <button type="submit" id="extract-btn">Extract</button>
    </form>
    <div class="meta" id="extract-meta"></div>
    <pre id="extract-out"></pre>
  </section>
  <footer>
    API: <a href="/openapi.json">openapi.json</a> ·
    <a href="/health">/health</a> · POST /search · POST /extract
  </footer>
</main>
<script>
const $ = (id) => document.getElementById(id);
$('tab-search').onclick = () => { $('panel-search').classList.remove('hidden'); $('panel-extract').classList.add('hidden'); $('tab-search').classList.add('active'); $('tab-extract').classList.remove('active'); };
$('tab-extract').onclick = () => { $('panel-extract').classList.remove('hidden'); $('panel-search').classList.add('hidden'); $('tab-extract').classList.add('active'); $('tab-search').classList.remove('active'); };

$('search-form').onsubmit = async (e) => {
  e.preventDefault();
  const q = $('q').value.trim();
  if (!q) return;
  $('search-btn').disabled = true;
  $('results').innerHTML = '';
  $('warnings').innerHTML = '';
  $('search-meta').textContent = 'Searching…';
  try {
    const res = await fetch('/search', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ query:q, max_results:8 }) });
    const data = await res.json();
    $('search-meta').textContent = (data.results||[]).length + ' results in ' + (data.response_time||0).toFixed(2) + 's';
    (data.warnings||[]).forEach(w => {
      const d = document.createElement('div'); d.className='warn'; d.textContent = w; $('warnings').appendChild(d);
    });
    (data.results||[]).forEach((r,i) => {
      const a = document.createElement('article');
      a.innerHTML = `<span class="score">${(r.score||0).toFixed(2)}</span><h2>${i+1}. ${escapeHtml(r.title||'')}</h2><a href="${escapeAttr(r.url||'')}" target="_blank" rel="noopener">${escapeHtml(r.url||'')}</a><p>${escapeHtml(r.content||'')}</p>`;
      $('results').appendChild(a);
    });
    if (!(data.results||[]).length) $('results').innerHTML = '<p class="meta">No results.</p>';
  } catch (err) {
    $('search-meta').textContent = 'Error: ' + err;
  } finally {
    $('search-btn').disabled = false;
  }
};

$('extract-form').onsubmit = async (e) => {
  e.preventDefault();
  const url = $('url').value.trim();
  $('extract-btn').disabled = true;
  $('extract-out').textContent = '';
  $('extract-meta').textContent = 'Extracting…';
  try {
    const res = await fetch('/extract', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ urls:[url] }) });
    const data = await res.json();
    const item = (data.results||[])[0] || {};
    $('extract-meta').textContent = (item.status||'?') + (item.error ? (' — ' + item.error) : '');
    $('extract-out').textContent = item.raw_content || '';
  } catch (err) {
    $('extract-meta').textContent = 'Error: ' + err;
  } finally {
    $('extract-btn').disabled = false;
  }
};

function escapeHtml(s){ return s.replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c])); }
function escapeAttr(s){ return escapeHtml(s).replace(/`/g,''); }
</script>
</body>
</html>
)HTML";

std::string openapi_json() {
  nlohmann::json doc;
  doc["openapi"] = "3.0.3";
  doc["info"] = {
      {"title", "WebSearchFree"},
      {"version", wsf::kVersion},
      {"description", "Keyless metasearch + extract API (Tavily-shaped). No API key required."},
  };
  doc["paths"]["/health"]["get"]["summary"] = "Health check";
  doc["paths"]["/health"]["get"]["responses"]["200"]["description"] = "OK";
  doc["paths"]["/search"]["post"]["summary"] = "Metasearch";
  doc["paths"]["/search"]["post"]["responses"]["200"]["description"] = "Search response";
  doc["paths"]["/search"]["get"]["summary"] = "Metasearch (query params)";
  doc["paths"]["/search"]["get"]["responses"]["200"]["description"] = "Search response";
  doc["paths"]["/extract"]["post"]["summary"] = "Extract page text";
  doc["paths"]["/extract"]["post"]["responses"]["200"]["description"] = "Extract results";
  doc["paths"]["/openapi.json"]["get"]["summary"] = "OpenAPI document";
  doc["paths"]["/openapi.json"]["get"]["responses"]["200"]["description"] = "OpenAPI JSON";
  return doc.dump(2);
}

int cmd_serve(std::vector<std::string> args) {
  std::string host = "127.0.0.1";
  int port = 8080;
  std::string default_searx = env_or_empty("WSF_SEARX_URL");
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--host" && i + 1 < args.size())
      host = args[++i];
    else if (args[i] == "--port" && i + 1 < args.size())
      port = std::stoi(args[++i]);
    else if (args[i] == "--searx-url" && i + 1 < args.size())
      default_searx = args[++i];
  }

  httplib::Server svr;

  auto options_handler = [](const httplib::Request&, httplib::Response& res) {
    set_cors(res);
    res.status = 204;
  };
  svr.Options("/search", options_handler);
  svr.Options("/extract", options_handler);
  svr.Options("/health", options_handler);

  svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
    set_cors(res);
    res.set_content(kUiHtml, "text/html; charset=utf-8");
  });

  svr.Get("/openapi.json", [](const httplib::Request&, httplib::Response& res) {
    set_cors(res);
    res.set_content(openapi_json(), "application/json");
  });

  svr.Get("/health", [default_searx](const httplib::Request&, httplib::Response& res) {
    set_cors(res);
    auto engines = wsf::available_engines();
    nlohmann::json j = {
        {"ok", true},
        {"service", "WebSearchFree"},
        {"version", wsf::kVersion},
        {"engines", engines},
        {"searx_configured", !default_searx.empty()},
    };
    res.set_content(j.dump(2), "application/json");
  });

  auto handle_search = [default_searx](const httplib::Request& req, httplib::Response& res) {
    set_cors(res);
    wsf::Options opt;
    opt.searx_url = default_searx;
    std::string query;
    try {
      if (req.method == "GET") {
        query = req.get_param_value("query");
        if (query.empty()) query = req.get_param_value("q");
        if (req.has_param("max_results")) opt.max_results = std::stoi(req.get_param_value("max_results"));
        if (req.has_param("include_raw_content")) {
          auto v = req.get_param_value("include_raw_content");
          opt.include_raw_content = (v == "1" || v == "true" || v == "True");
        }
        if (req.has_param("searx_url")) opt.searx_url = req.get_param_value("searx_url");
        if (req.has_param("timeout_ms")) opt.timeout_ms = std::stoi(req.get_param_value("timeout_ms"));
      } else {
        auto body = nlohmann::json::parse(req.body.empty() ? "{}" : req.body);
        query = body.value("query", body.value("q", ""));
        opt.max_results = body.value("max_results", opt.max_results);
        opt.include_raw_content = body.value("include_raw_content", false);
        opt.timeout_ms = body.value("timeout_ms", opt.timeout_ms);
        if (body.contains("searx_url") && body["searx_url"].is_string()) {
          opt.searx_url = body["searx_url"].get<std::string>();
        }
        if (body.contains("engines") && body["engines"].is_array()) {
          opt.engines.clear();
          for (const auto& e : body["engines"]) opt.engines.push_back(e.get<std::string>());
        }
      }
    } catch (const std::exception& ex) {
      res.status = 400;
      res.set_content(nlohmann::json{{"error", ex.what()}}.dump(), "application/json");
      return;
    }
    if (query.empty()) {
      res.status = 400;
      res.set_content(R"({"error":"query is required"})", "application/json");
      return;
    }
    auto resp = wsf::search(query, opt);
    res.set_content(wsf::to_json(resp).dump(2), "application/json");
  };

  svr.Post("/search", handle_search);
  svr.Get("/search", handle_search);

  svr.Post("/extract", [](const httplib::Request& req, httplib::Response& res) {
    set_cors(res);
    try {
      auto body = nlohmann::json::parse(req.body.empty() ? "{}" : req.body);
      std::vector<std::string> urls;
      if (body.contains("urls") && body["urls"].is_array()) {
        for (const auto& u : body["urls"]) {
          if (u.is_string()) urls.push_back(u.get<std::string>());
        }
      } else {
        auto url = body.value("url", "");
        if (!url.empty()) urls.push_back(url);
      }
      if (urls.empty()) {
        res.status = 400;
        res.set_content(R"({"error":"url or urls is required"})", "application/json");
        return;
      }
      constexpr size_t kMaxUrls = 10;
      if (urls.size() > kMaxUrls) urls.resize(kMaxUrls);
      int timeout_ms = body.value("timeout_ms", 8000);
      int concurrency = body.value("concurrency", 4);
      auto results = wsf::extract_many(urls, timeout_ms, concurrency);
      res.set_content(wsf::to_json(results).dump(2), "application/json");
    } catch (const std::exception& ex) {
      res.status = 400;
      res.set_content(nlohmann::json{{"error", ex.what()}}.dump(), "application/json");
    }
  });

  std::cout << "WebSearchFree " << wsf::kVersion << " listening on http://" << host << ":" << port
            << "\n  UI /  ·  POST /search  ·  POST /extract  ·  GET /health  ·  GET /openapi.json\n";
  if (!svr.listen(host, port)) {
    std::cerr << "Failed to bind " << host << ":" << port << "\n";
    return 1;
  }
  return 0;
}
#endif

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
    return 2;
  }
  std::string cmd = argv[1];
  std::vector<std::string> args;
  for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);

  if (cmd == "search") return cmd_search(std::move(args));
  if (cmd == "extract") return cmd_extract(std::move(args));
#if defined(WSF_HAS_SERVER)
  if (cmd == "serve") return cmd_serve(std::move(args));
#endif
  if (cmd == "-h" || cmd == "--help" || cmd == "help") {
    print_usage();
    return 0;
  }
  if (cmd == "-v" || cmd == "--version" || cmd == "version") {
    std::cout << "WebSearchFree " << wsf::kVersion << "\n";
    return 0;
  }
  // Convenience: `wsf "some query"` → search
  args.insert(args.begin(), cmd);
  return cmd_search(std::move(args));
}

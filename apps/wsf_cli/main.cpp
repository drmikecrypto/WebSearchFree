#include "wsf/wsf.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(WSF_HAS_SERVER)
#  include "httplib.h"
#endif

namespace {

void print_usage() {
  std::cerr
      << "WebSearchFree (wsf) — free, keyless metasearch for AI\n\n"
      << "Usage:\n"
      << "  wsf search <query> [--max N] [--raw] [--json] [--engines ddg,brave,wikipedia]\n"
      << "  wsf extract <url> [--json]\n"
#if defined(WSF_HAS_SERVER)
      << "  wsf serve [--host 127.0.0.1] [--port 8080]\n"
#endif
      << "\nNo API keys required.\n";
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

int cmd_search(std::vector<std::string> args) {
  if (args.empty()) {
    print_usage();
    return 2;
  }
  wsf::Options opt;
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

  auto resp = wsf::search(query, opt);
  if (as_json) {
    std::cout << wsf::to_json(resp).dump(2) << "\n";
  } else {
    for (size_t i = 0; i < resp.results.size(); ++i) {
      const auto& r = resp.results[i];
      std::cout << (i + 1) << ". " << r.title << "\n   " << r.url << "\n   " << r.content << "\n\n";
    }
  }
  return 0;
}

int cmd_extract(std::vector<std::string> args) {
  bool as_json = false;
  std::string url;
  for (const auto& a : args) {
    if (a == "--json")
      as_json = true;
    else if (!a.empty() && a[0] != '-')
      url = a;
  }
  if (url.empty()) {
    print_usage();
    return 2;
  }
  auto text = wsf::extract(url);
  if (as_json) {
    nlohmann::json j = {{"url", url}, {"raw_content", text}};
    std::cout << j.dump(2) << "\n";
  } else {
    std::cout << text << "\n";
  }
  return 0;
}

#if defined(WSF_HAS_SERVER)
int cmd_serve(std::vector<std::string> args) {
  std::string host = "127.0.0.1";
  int port = 8080;
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--host" && i + 1 < args.size()) host = args[++i];
    else if (args[i] == "--port" && i + 1 < args.size()) port = std::stoi(args[++i]);
  }

  httplib::Server svr;

  svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
    res.set_content(R"({"ok":true,"service":"WebSearchFree"})", "application/json");
  });

  auto handle_search = [](const httplib::Request& req, httplib::Response& res) {
    wsf::Options opt;
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
      } else {
        auto body = nlohmann::json::parse(req.body.empty() ? "{}" : req.body);
        query = body.value("query", body.value("q", ""));
        opt.max_results = body.value("max_results", opt.max_results);
        opt.include_raw_content = body.value("include_raw_content", false);
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
    // Tavily-shaped envelope
    auto j = wsf::to_json(resp);
    j["answer"] = nullptr;
    j["images"] = nlohmann::json::array();
    j["follow_up_questions"] = nullptr;
    j["response_time"] = 0;
    res.set_content(j.dump(2), "application/json");
  };

  svr.Post("/search", handle_search);
  svr.Get("/search", handle_search);

  svr.Post("/extract", [](const httplib::Request& req, httplib::Response& res) {
    try {
      auto body = nlohmann::json::parse(req.body.empty() ? "{}" : req.body);
      std::string url;
      if (body.contains("urls") && body["urls"].is_array() && !body["urls"].empty()) {
        url = body["urls"][0].get<std::string>();
      } else {
        url = body.value("url", "");
      }
      if (url.empty()) {
        res.status = 400;
        res.set_content(R"({"error":"url is required"})", "application/json");
        return;
      }
      auto text = wsf::extract(url);
      nlohmann::json out = {
          {"results", nlohmann::json::array({{{"url", url}, {"raw_content", text}}})},
      };
      res.set_content(out.dump(2), "application/json");
    } catch (const std::exception& ex) {
      res.status = 400;
      res.set_content(nlohmann::json{{"error", ex.what()}}.dump(), "application/json");
    }
  });

  std::cout << "WebSearchFree listening on http://" << host << ":" << port
            << "  (POST /search, POST /extract, GET /health)\n";
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
  // Convenience: `wsf "some query"` → search
  args.insert(args.begin(), cmd);
  return cmd_search(std::move(args));
}

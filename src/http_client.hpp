#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wsf::detail {

struct HttpResponse {
  long status = 0;
  std::string body;
  std::string final_url;
  std::string error;
};

struct HttpRequest {
  std::string url;
  std::string method = "GET";
  std::string body;
  std::vector<std::pair<std::string, std::string>> headers;
  int timeout_ms = 8000;
};

HttpResponse http_request(const HttpRequest& req);

inline HttpResponse http_get(std::string_view url, int timeout_ms = 8000) {
  HttpRequest req;
  req.url = std::string(url);
  req.timeout_ms = timeout_ms;
  return http_request(req);
}

inline HttpResponse http_post_form(std::string_view url, std::string_view form_body,
                                   int timeout_ms = 8000) {
  HttpRequest req;
  req.url = std::string(url);
  req.method = "POST";
  req.body = std::string(form_body);
  req.timeout_ms = timeout_ms;
  req.headers.emplace_back("Content-Type", "application/x-www-form-urlencoded");
  return http_request(req);
}

std::string default_user_agent();

}  // namespace wsf::detail

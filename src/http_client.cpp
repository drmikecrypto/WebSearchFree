#include "http_client.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>

#if defined(WSF_USE_WINHTTP)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <winhttp.h>
#elif defined(WSF_USE_CURL)
#  include <curl/curl.h>
#endif

namespace wsf::detail {
namespace {

std::wstring utf8_to_wide(std::string_view s) {
  if (s.empty()) return {};
#if defined(_WIN32)
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring out(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
  return out;
#else
  return std::wstring(s.begin(), s.end());
#endif
}

std::string wide_to_utf8(std::wstring_view s) {
  if (s.empty()) return {};
#if defined(_WIN32)
  int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr,
                              nullptr);
  std::string out(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr,
                      nullptr);
  return out;
#else
  return std::string(s.begin(), s.end());
#endif
}

bool should_retry(const HttpResponse& resp) {
  if (!resp.error.empty()) return true;
  if (resp.status == 429) return true;
  if (resp.status >= 500 && resp.status <= 599) return true;
  return false;
}

#if defined(WSF_USE_WINHTTP)

HttpResponse http_request_once(const HttpRequest& req) {
  HttpResponse out;
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwSchemeLength = static_cast<DWORD>(-1);
  parts.dwHostNameLength = static_cast<DWORD>(-1);
  parts.dwUrlPathLength = static_cast<DWORD>(-1);
  parts.dwExtraInfoLength = static_cast<DWORD>(-1);

  std::wstring wurl = utf8_to_wide(req.url);
  if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts)) {
    out.error = "Failed to parse URL";
    return out;
  }

  std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  if (parts.dwExtraInfoLength > 0 && parts.lpszExtraInfo) {
    path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
  }
  if (path.empty()) path = L"/";

  HINTERNET session = WinHttpOpen(utf8_to_wide(default_user_agent()).c_str(),
                                  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    out.error = "WinHttpOpen failed";
    return out;
  }

  DWORD timeout = static_cast<DWORD>(req.timeout_ms);
  WinHttpSetTimeouts(session, timeout, timeout, timeout, timeout);

  // Follow redirects (parity with libcurl CURLOPT_FOLLOWLOCATION).
  DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
  WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy,
                   sizeof(redirect_policy));

  INTERNET_PORT port = parts.nPort;
  HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
  if (!connect) {
    out.error = "WinHttpConnect failed";
    WinHttpCloseHandle(session);
    return out;
  }

  DWORD flags = (parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
  std::wstring method = utf8_to_wide(req.method);
  HINTERNET request =
      WinHttpOpenRequest(connect, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
                         WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!request) {
    out.error = "WinHttpOpenRequest failed";
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return out;
  }

  std::wstring headers = L"Accept: text/html,application/xhtml+xml,application/json;q=0.9,*/*;q=0.8\r\n";
  headers += L"Accept-Language: en-US,en;q=0.9\r\n";
  for (const auto& [k, v] : req.headers) {
    headers += utf8_to_wide(k);
    headers += L": ";
    headers += utf8_to_wide(v);
    headers += L"\r\n";
  }

  BOOL ok = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(-1L),
                               req.body.empty() ? WINHTTP_NO_REQUEST_DATA
                                                : const_cast<char*>(req.body.data()),
                               static_cast<DWORD>(req.body.size()),
                               static_cast<DWORD>(req.body.size()), 0);
  if (!ok || !WinHttpReceiveResponse(request, nullptr)) {
    out.error = "HTTP request failed";
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return out;
  }

  DWORD status = 0;
  DWORD status_size = sizeof(status);
  WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
  out.status = static_cast<long>(status);

  DWORD buf_len = 0;
  WinHttpQueryOption(request, WINHTTP_OPTION_URL, nullptr, &buf_len);
  if (buf_len > 0) {
    std::wstring final_w(buf_len / sizeof(wchar_t), L'\0');
    if (WinHttpQueryOption(request, WINHTTP_OPTION_URL, final_w.data(), &buf_len)) {
      size_t chars = buf_len / sizeof(wchar_t);
      if (chars > 0 && final_w[chars - 1] == L'\0') --chars;
      final_w.resize(chars);
      out.final_url = wide_to_utf8(final_w);
    }
  }
  if (out.final_url.empty()) out.final_url = req.url;

  for (;;) {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(request, &avail)) break;
    if (avail == 0) break;
    std::string chunk(avail, '\0');
    DWORD read = 0;
    if (!WinHttpReadData(request, chunk.data(), avail, &read)) break;
    chunk.resize(read);
    out.body += chunk;
  }

  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);
  return out;
}

#elif defined(WSF_USE_CURL)

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* out = static_cast<std::string*>(userdata);
  out->append(ptr, size * nmemb);
  return size * nmemb;
}

HttpResponse http_request_once(const HttpRequest& req) {
  HttpResponse out;
  CURL* curl = curl_easy_init();
  if (!curl) {
    out.error = "curl_easy_init failed";
    return out;
  }

  struct curl_slist* hdrs = nullptr;
  hdrs = curl_slist_append(hdrs, "Accept: text/html,application/xhtml+xml,application/json;q=0.9,*/*;q=0.8");
  for (const auto& [k, v] : req.headers) {
    std::string line = k + ": " + v;
    hdrs = curl_slist_append(hdrs, line.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
  curl_easy_setopt(curl, CURLOPT_USERAGENT, default_user_agent().c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(req.timeout_ms));
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out.body);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

  if (req.method == "POST") {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(req.body.size()));
  }

  CURLcode rc = curl_easy_perform(curl);
  if (rc != CURLE_OK) {
    out.error = curl_easy_strerror(rc);
  } else {
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    out.status = status;
    char* effective = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective);
    if (effective) out.final_url = effective;
  }

  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);
  if (out.final_url.empty()) out.final_url = req.url;
  return out;
}

#else
#  error "No HTTP backend configured"
#endif

}  // namespace

std::string default_user_agent() {
  return "WebSearchFree/0.2 (+https://github.com/drmikecrypto/WebSearchFree; research)";
}

HttpResponse http_request(const HttpRequest& req) {
  HttpResponse out = http_request_once(req);
  if (should_retry(out)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    out = http_request_once(req);
  }
  return out;
}

}  // namespace wsf::detail

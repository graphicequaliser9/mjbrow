/**
 * @file HttpClient.cpp
 * @brief HTTP client using WinHTTP with WinInet fallback.
 * @details This module handles HTTP/HTTPS requests using pure WinHTTP,
 *          falling back to WinInet when WinHTTP is unavailable.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "net/HttpClient.h"
#include <string>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include "util/Logging.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <wininet.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "wininet.lib")
#endif

namespace net {

HttpClient::HttpClient() = default;

HttpClient::~HttpClient() = default;

bool HttpClient::parseUrl(const std::string& url, ParsedUrl& out) {
    if (url.empty()) return false;

    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) return false;

    out.scheme = url.substr(0, schemeEnd);
    std::string rest = url.substr(schemeEnd + 3);

    size_t pathStart = rest.find('/');
    if (pathStart == std::string::npos) {
        out.host = rest;
        out.path = "/";
    } else {
        out.host = rest.substr(0, pathStart);
        out.path = rest.substr(pathStart);
    }

    size_t portStart = out.host.find(':');
    if (portStart != std::string::npos) {
        std::string portStr = out.host.substr(portStart + 1);
        out.port = std::stoi(portStr);
        out.host = out.host.substr(0, portStart);
    } else {
        out.port = (out.scheme == "https") ? 443 : 80;
    }

    if (out.port == 0) return false;
    return true;
}

#ifdef _WIN32
 static std::string wideToAnsi(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

static std::wstring to_wstring_utf8(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len == 0) return {};
    std::wstring w(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    return w;
}
#endif

static std::vector<std::string> splitLines(const std::string& str) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < str.size()) {
        size_t end = str.find("\r\n", start);
        if (end == std::string::npos) {
            lines.push_back(str.substr(start));
            break;
        }
        lines.push_back(str.substr(start, end - start));
        start = end + 2;
    }
    return lines;
}

static void parseRawHeaders(const std::string& rawHeaders, std::map<std::string, std::string>& out) {
    auto lines = splitLines(rawHeaders);
    for (const auto& line : lines) {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            if (!value.empty() && value.front() == ' ') value.erase(value.begin());
            if (!value.empty() && value.back() == '\r') value.pop_back();
            out[key] = value;
        }
    }
}

#ifdef _WIN32
HttpResponse HttpClient::fetchWinHTTP(const ParsedUrl& parsed, const std::string& method) {
    HttpResponse response;
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;

    hSession = WinHttpOpen(L"NitrogenBrowser/1.0",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return response;

    DWORD timeoutMs = 10000;
    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    std::wstring wHost = to_wstring_utf8(parsed.host);

    hConnect = WinHttpConnect(hSession,
                              wHost.c_str(),
                              static_cast<INTERNET_PORT>(parsed.port),
                              0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return response;
    }

    DWORD flags = (parsed.scheme == "https") ? WINHTTP_FLAG_SECURE : 0;
    std::wstring wMethod = to_wstring_utf8(method);
    std::wstring wPath = to_wstring_utf8(parsed.path);
    hRequest = WinHttpOpenRequest(hConnect,
                                  wMethod.c_str(),
                                  wPath.c_str(),
                                  nullptr,
                                  WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    BOOL result = WinHttpSendRequest(hRequest,
                                     WINHTTP_NO_ADDITIONAL_HEADERS,
                                     0,
                                     WINHTTP_NO_REQUEST_DATA,
                                     0,
                                     0,
                                     0);
    if (!result) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    result = WinHttpReceiveResponse(hRequest, nullptr);
    if (!result) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &statusCode,
                        &statusSize,
                        WINHTTP_NO_HEADER_INDEX);
    response.status = static_cast<int>(statusCode);

    WCHAR rawHeadersW[4096] = {0};
    DWORD rawHeadersSize = sizeof(rawHeadersW);
    if (WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_RAW_HEADERS_CRLF,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            rawHeadersW,
                            &rawHeadersSize,
                            WINHTTP_NO_HEADER_INDEX)) {
        std::string rawHeaders = wideToAnsi(rawHeadersW);
        parseRawHeaders(rawHeaders, response.headers);
    }

    constexpr DWORD chunkSize = 8192;
    std::vector<uint8_t> buffer;
    DWORD bytesAvailable = 0;

    do {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) break;
        if (bytesAvailable == 0) break;

        size_t oldSize = buffer.size();
        buffer.resize(oldSize + bytesAvailable);
        DWORD downloaded = 0;
        if (!WinHttpReadData(hRequest, buffer.data() + oldSize, bytesAvailable, &downloaded)) {
            break;
        }
        if (downloaded < bytesAvailable) {
            buffer.resize(oldSize + downloaded);
        }
    } while (bytesAvailable > 0);

    response.body = std::move(buffer);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

HttpResponse HttpClient::fetchWinInet(const ParsedUrl& parsed, const std::string& method) {
    HttpResponse response;

    DWORD timeoutMs = 10000;

    HINTERNET hSession = InternetOpenW(L"NitrogenBrowser/1.0",
                                       INTERNET_OPEN_TYPE_PRECONFIG,
                                       nullptr,
                                       nullptr,
                                       0);
    if (!hSession) return response;

    InternetSetOptionW(hSession, INTERNET_OPTION_CONNECT_TIMEOUT,
                       (LPVOID)&timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(hSession, INTERNET_OPTION_RECEIVE_TIMEOUT,
                       (LPVOID)&timeoutMs, sizeof(timeoutMs));

    std::wstring wHost = to_wstring_utf8(parsed.host);
    std::wstring wPath = to_wstring_utf8(parsed.path);
    std::wstring wMethod = to_wstring_utf8(method);

    HINTERNET hConnect = InternetConnectW(hSession,
                                          wHost.c_str(),
                                          static_cast<INTERNET_PORT>(parsed.port),
                                          nullptr,
                                          nullptr,
                                          INTERNET_SERVICE_HTTP,
                                          0,
                                          0);
    if (!hConnect) {
        InternetCloseHandle(hSession);
        return response;
    }

    DWORD flags = (parsed.scheme == "https") ? INTERNET_FLAG_SECURE : 0;
    flags |= INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;

    HINTERNET hRequest = HttpOpenRequestW(hConnect,
                                          wMethod.c_str(),
                                          wPath.c_str(),
                                          nullptr,
                                          WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          flags,
                                          0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hSession);
        return response;
    }

    BOOL result = HttpSendRequestW(hRequest,
                                   WINHTTP_NO_ADDITIONAL_HEADERS,
                                   0,
                                   WINHTTP_NO_REQUEST_DATA,
                                   0);
    if (!result) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hSession);
        return response;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    HttpQueryInfoW(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                   &statusCode, &statusSize, nullptr);
    response.status = static_cast<int>(statusCode);

    WCHAR rawHeadersW[4096] = {0};
    DWORD rawHeadersSize = sizeof(rawHeadersW);
    if (HttpQueryInfoW(hRequest, HTTP_QUERY_RAW_HEADERS_CRLF,
                       rawHeadersW, &rawHeadersSize, nullptr)) {
        std::string rawHeaders = wideToAnsi(rawHeadersW);
        parseRawHeaders(rawHeaders, response.headers);
    }

    constexpr DWORD chunkSize = 8192;
    std::vector<uint8_t> buffer;
    DWORD bytesRead = 0;

    do {
        bytesRead = 0;
        char chunk[chunkSize];
        if (!InternetReadFile(hRequest, chunk, chunkSize, &bytesRead)) break;
        if (bytesRead == 0) break;

        size_t oldSize = buffer.size();
        buffer.resize(oldSize + bytesRead);
        memcpy(buffer.data() + oldSize, chunk, bytesRead);
    } while (bytesRead > 0);

    response.body = std::move(buffer);

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hSession);

    return response;
}
#endif // _WIN32

HttpResponse HttpClient::sendRequest(const std::string& url, const std::string& method) {
    ParsedUrl parsed;
    if (!parseUrl(url, parsed)) {
        util::Log(util::LogLevel::Warn, "HttpClient: failed to parse URL: " + url + "\n");
        return HttpResponse{};
    }

#ifdef _WIN32
    HttpResponse response = fetchWinHTTP(parsed, method);
    if (response.status == 0 || response.body.empty()) {
        util::Log(util::LogLevel::Info, "HttpClient: WinHTTP failed or returned empty, trying WinInet fallback\n");
        response = fetchWinInet(parsed, method);
    }

    if (response.status != 0) {
        util::Log(util::LogLevel::Info,
                  "HttpClient: " + url + " → " + std::to_string(response.status) +
                  " (" + std::to_string(response.body.size()) + " bytes)\n");
    }

    return response;
#else
    // Cross-platform / headless stub: network fetches are only implemented on
    // Windows (WinHTTP/WinInet).  Non-Windows builds return an empty response so
    // the browser can still load pages via Tab::loadHTML for testing.
    (void)method;
    util::Log(util::LogLevel::Warn,
              "HttpClient: network disabled on this platform, no body for " + url + "\n");
    return HttpResponse{};
#endif
}

} // namespace net

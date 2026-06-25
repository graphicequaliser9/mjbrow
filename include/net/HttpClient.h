/**
 * @file net/HttpClient.h
 * @brief HTTP client using WinHTTP (cross-platform stub).
 * @details This module handles HTTP/HTTPS requests.  HttpResponse is defined
 *          inline so callers (Tab, URLBar, …) can read status, headers, and body
 *          without pulling in the private cpp implementation.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef NET_HTTPCLIENT_H
#define NET_HTTPCLIENT_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace net {

/**
 * @struct HttpResponse
 * @brief Aggregates one HTTP response: status line, header map, and body bytes.
 */
struct HttpResponse {
    int status{0};
    std::map<std::string, std::string> headers;
    /**
     * @brief Raw response body bytes (decoded from transport bytes).
     */
    std::vector<uint8_t> body;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    /**
     * @brief Sends an HTTP request and returns the response.
     * @param url    The URL to request.
     * @param method HTTP method (GET, POST, etc.).
     * @return Full HTTP response.
     */
    HttpResponse sendRequest(const std::string& url, const std::string& method = "GET");

private:
    struct ParsedUrl {
        std::string scheme;
        std::string host;
        int port{0};
        std::string path;
    };

    /**
     * @brief Parses a URL into its components.
     * @param url   The URL to parse.
     * @param out   Output parsed components.
     * @return True if parsing succeeded.
     */
    bool parseUrl(const std::string& url, ParsedUrl& out);

    /**
     * @brief Performs the request using WinHTTP.
     */
    HttpResponse fetchWinHTTP(const ParsedUrl& parsed, const std::string& method);

    /**
     * @brief Performs the request using WinInet as fallback.
     */
    HttpResponse fetchWinInet(const ParsedUrl& parsed, const std::string& method);
};

} // namespace net

#endif // NET_HTTPCLIENT_H

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
    std::vector<unsigned char> body;
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
    // Placeholder for internal state
};

} // namespace net

#endif // NET_HTTPCLIENT_H

/**
 * @file net/HttpClient.h
 * @brief HTTP client using WinHTTP.
 * @details This module handles HTTP/HTTPS requests.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef NET_HTTPCLIENT_H
#define NET_HTTPCLIENT_H

#include <string>

namespace net {

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    /// @brief Sends an HTTP request and returns the response.
    /// @param url The URL to request.
    /// @param method The HTTP method (GET, POST, etc.).
    /// @return The HTTP response.
    class HttpResponse sendRequest(const std::string& url, const std::string& method = "GET");

private:
    // Placeholder for internal state
};

} // namespace net

#endif // NET_HTTPCLIENT_H
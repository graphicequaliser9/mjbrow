#include "../include/net/HttpNetworking.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include <zlib.h>

// ═══════════════════════════════════════════════════════════
// BSD socket helpers  (non-Windows)
// ═══════════════════════════════════════════════════════════

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace {

// Resolve host, open a TCP socket, connect – returns fd or -1 on failure.
inline int connect_socket(const char *host, int port) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[8];
    std::snprintf(portStr, sizeof(portStr), "%d", port);

    if (getaddrinfo(host, portStr, &hints, &res) != 0) return -1;

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return -1; }

    struct timeval tv{30, 0};
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(sock, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen)) < 0) {
        close(sock); freeaddrinfo(res); return -1;
    }
    freeaddrinfo(res);
    return sock;
}

// Build a raw HTTP/1.1 request string from parsed URL + request headers.
inline std::string build_request_string(const net::UrlParser   &parser,
                                        const net::HttpRequest &req,
                                        bool include_body) {
    std::string path = parser.encoded_path_query();

    std::string hostH = parser.host();
    if ((parser.scheme() == net::Scheme::Http  && parser.port() != 80) ||
        (parser.scheme() == net::Scheme::Https && parser.port() != 443))
        hostH += ":" + parser.port_str();

    std::string headerBlock =
        std::string(req.method()) + " " + path + " HTTP/1.1\r\n"
        "Host: " + hostH + "\r\n";

    // Include caller-supplied headers (respect original case; skip Host sent above).
    for (auto &[k, v] : req.headers().entries()) {
        std::string lk;
        lk.reserve(k.size());
        for (char c : k) lk.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))));
        if (lk == "host") continue;
        headerBlock += k + ": " + v + "\r\n";
    }

    if (include_body && !req.body().empty()) {
        headerBlock += "Content-Length: "
                     + std::to_string(req.body().size()) + "\r\n";
    }

    headerBlock += "Connection: close\r\n\r\n";
    if (include_body && !req.body().empty())
        headerBlock += req.body();
    return headerBlock;
}

// Send the request bytes, then read the raw response.
inline std::string send_and_receive(int sock, const std::string &req_bytes) {
    send(sock, req_bytes.data(), static_cast<int>(req_bytes.size()), 0);

    std::string raw;
    char buf[8192];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0)
        raw.append(buf, static_cast<size_t>(n));
    close(sock);
    return raw;
}

// Split raw HTTP response into <header-block, body>.
// Respects Content-Length for exact body slicing; falls back gracefully.
struct RawParts {
    std::string headers_raw;
    std::string body;
};

inline RawParts split_headers_body(const std::string &raw) {
    RawParts out{};
    size_t hdrEnd = raw.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) return out;

    out.headers_raw = raw.substr(0, hdrEnd);

    // Find Content-Length in the header block
    long contentLength = -1;
    const std::string clKey = "Content-Length:";
    size_t pos = 0;
    while (pos < hdrEnd) {
        size_t lf  = raw.find('\n', pos);
        size_t end = (lf == std::string::npos) ? hdrEnd : lf;
        std::string_view line{raw.data() + pos, end - pos};
        pos = (lf == std::string::npos) ? raw.size() : lf + 1;

        std::string lk;
        lk.reserve(line.size());
        for (char c : line) lk.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))));
        size_t clPos = lk.find("content-length:");
        if (clPos != std::string::npos) {
            const char *p = line.data() + clPos + std::strlen("content-length:");
            while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p;
            try { contentLength = std::stol(p); } catch (...) {}
            break;
        }
    }

    const char *bodyStart = raw.data() + hdrEnd + 4;
    size_t avail = raw.size() - (hdrEnd + 4);
    if (contentLength >= 0) {
        size_t take = static_cast<size_t>(
            std::min<long>(contentLength, static_cast<long>(avail)));
        out.body.assign(bodyStart, take);
    } else {
        out.body.assign(bodyStart, avail);
    }
    return out;
}

} // anonymous
#endif // !_WIN32

// ═══════════════════════════════════════════════════════════
// Free helpers – RFC-date parsing
// ═══════════════════════════════════════════════════════════

namespace {

inline std::tm parse_rfcdate_parts(std::string_view sv,
                                    int &day, int &year, int &hour, int &min, int &sec,
                                    char monStr[4])
{
    std::memset(&monStr[0], 0, 4);
    day = year = hour = min = sec = 0;
    std::string str{sv};

    constexpr const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                      "Jul","Aug","Sep","Oct","Nov","Dec"};

    int r = std::sscanf(str.c_str(),
        "%*[^0-9]%d %3s %d %d:%d:%d GMT", &day, monStr, &year, &hour, &min, &sec);
    if (r < 6) {
        r = std::sscanf(str.c_str(), "%*[^0-9]%d-%3s-%d %d:%d:%d GMT",
                        &day, monStr, &year, &hour, &min, &sec);
    }
    if (r < 6) {
        r = std::sscanf(str.c_str(), "%*[^-0-9]%3s %d %d:%d:%d %d",
                        monStr, &day, &hour, &min, &sec, &year);
    }

    int month = 0;
    for (int i = 0; i < 12; ++i)
        if (std::string{months[i]} == monStr) { month = i; break; }
    if (year <  70) year += 2000;
    else if (year < 100) year += 1900;

    std::tm tm{};
    tm.tm_mday  = day;
    tm.tm_mon   = month;
    tm.tm_year  = year - 1900;
    tm.tm_hour  = hour;
    tm.tm_min   = min;
    tm.tm_sec   = sec;
    tm.tm_isdst = -1;
    return tm;
}

// Parse "HTTP/1.1 200 OK" → (code, text)
inline net::HttpCache::StatusLineResult
parse_status_line(std::string_view raw) {
    net::HttpCache::StatusLineResult r{200, "OK"};

    auto trimLeft = [](std::string_view s) {
        size_t p = 0;
        while (p < s.size() &&
               std::isspace(static_cast<unsigned char>(s[p]))) ++p;
        return s.substr(p);
    };
    auto trimRight = [](std::string_view s) {
        size_t p = s.size();
        while (p > 0 &&
               std::isspace(static_cast<unsigned char>(s[p - 1]))) --p;
        return s.substr(0, p);
    };

    raw = trimLeft(raw);
    // Expect "HTTP/x.y" as the first token.
    if (raw.size() < 7 || raw.substr(0, 4) != "HTTP") return r;

    // After "HTTP/x.y", skip whitespace, read status-code digits.
    size_t pos = 4;
    while (pos < raw.size() &&
           std::isspace(static_cast<unsigned char>(raw[pos]))) ++pos;
    if (!std::isdigit(static_cast<unsigned char>(raw[pos]))) return r;

    size_t codeStart = pos;
    while (pos < raw.size() &&
           std::isdigit(static_cast<unsigned char>(raw[pos]))) ++pos;
    try { r.code = std::stoi(std::string{raw.substr(codeStart, pos - codeStart)}); }
    catch (...) {}

    // Remaining text is the reason phrase.
    while (pos < raw.size() &&
           std::isspace(static_cast<unsigned char>(raw[pos]))) ++pos;
    r.text = std::string{trimRight(raw.substr(pos))};
    return r;
}

// Inflate helper – raw deflate (no zlib/gzip header).
inline std::string inflate_deflate_raw(const std::string &src) {
    if (src.empty()) return {};

    z_stream strm{};
    strm.next_in  = const_cast<Bytef *>(
                        reinterpret_cast<const Bytef *>(src.data()));
    strm.avail_in = static_cast<uInt>(src.size());
    inflateInit2(&strm, -MAX_WBITS);  // raw deflate

    std::string out;
    char buf[1 << 16];
    do {
        strm.next_out  = reinterpret_cast<Bytef *>(buf);
        strm.avail_out = sizeof(buf);
        int ret = inflate(&strm, Z_FINISH);
        if (ret != Z_OK && ret != Z_STREAM_END) break;
        out.append(buf, sizeof(buf) - strm.avail_out);
    } while (strm.avail_out == 0);

    inflateEnd(&strm);
    return out;
}

// Inflate helper – gzip wrapper.
inline std::string inflate_gzip(const std::string &src) {
    if (src.empty()) return {};

    z_stream strm{};
    strm.next_in  = const_cast<Bytef *>(
                        reinterpret_cast<const Bytef *>(src.data()));
    strm.avail_in = static_cast<uInt>(src.size());
    inflateInit2(&strm, 16 + MAX_WBITS);  // gzip

    std::string out;
    char buf[1 << 16];
    do {
        strm.next_out  = reinterpret_cast<Bytef *>(buf);
        strm.avail_out = sizeof(buf);
        int ret = inflate(&strm, Z_FINISH);
        if (ret != Z_OK && ret != Z_STREAM_END) break;
        out.append(buf, sizeof(buf) - strm.avail_out);
    } while (strm.avail_out == 0);

    inflateEnd(&strm);
    return out;
}

} // anonymous

// ═══════════════════════════════════════════════════════════
// net::HttpClient static methods
// ═══════════════════════════════════════════════════════════

void net::HttpClient::init() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

void net::HttpClient::shutdown() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// ───────────────────────────────────────────────────────────
// BSD-socket GET / HEAD / POST entry points
// ───────────────────────────────────────────────────────────

net::HttpResponse
net::HttpClient::get(const net::HttpRequest &req,
                     int maxRedirects,
                     std::optional<std::chrono::seconds> timeout)
{
    return detail_socket(req, /*include_body=*/true, maxRedirects, timeout);
}

net::HttpResponse
net::HttpClient::head(const net::HttpRequest &req,
                      int maxRedirects,
                      std::optional<std::chrono::seconds> timeout)
{
    return detail_socket(req, /*include_body=*/false, maxRedirects, timeout);
}

net::HttpResponse
net::HttpClient::post(const net::HttpRequest &req,
                      int maxRedirects,
                      std::optional<std::chrono::seconds> timeout)
{
    return detail_socket(req, /*include_body=*/true, maxRedirects, timeout);
}

// ───────────────────────────────────────────────────────────
// get_cached  –  cache-aware GET
// ───────────────────────────────────────────────────────────
// Strategy:
//   1. Probe HttpCache::lookup() first.
//   2. If a fresh (non-stale) copy exists → return immediately.
//   3. If a stale copy exists AND we got conditional headers → send network
//      request; on 304 Not Modified → return the stale body.
//   4. If no entry exists (or no conditional headers possible) → network fetch.
//   5. On success store the fresh response back into the cache.

net::HttpResponse
net::HttpClient::get_cached(const net::HttpRequest &req,
                            std::chrono::seconds       stale_after,
                            bool                       use_cache,
                            std::optional<std::chrono::seconds> timeout)
{
    const std::string &url = req.url();

    // Build a mutable copy for conditional-request injection.
    net::HttpRequest mutable_req = req;

    long       fresh_s  = static_cast<long>(stale_after.count());
    bool       has_cond = false;
    auto       cached   = HttpCache::lookup(url, &mutable_req, &has_cond, &fresh_s);

    // Fresh or fresh enough → use cached copy directly.
    if (cached && fresh_s >= 0) {
        net::HeaderList hdrs;
        if (!cached->etag.empty())
            hdrs.add("ETag", cached->etag);
        if (!cached->last_modified.empty())
            hdrs.add("Last-Modified", cached->last_modified);
        return net::HttpResponse{200, "OK (cached)",
                                 std::move(hdrs), cached->cached_body};
    }

    // Stale or absent → network fetch.
    auto resp = get(mutable_req, /*maxRedirects=*/5, timeout);

    // Server returned 304 → cached copy is still valid.
    if (has_cond && resp.status_code() == 304) {
        cached = HttpCache::lookup(url, nullptr, nullptr, nullptr);
        if (cached) {
            net::HeaderList hdrs;
            if (!cached->etag.empty())
                hdrs.add("ETag", cached->etag);
            if (!cached->last_modified.empty())
                hdrs.add("Last-Modified", cached->last_modified);
            resp = net::HttpResponse{304, "Not Modified",
                                      std::move(hdrs), cached->cached_body};
        }
        return resp;
    }

    if (resp.ok()) HttpCache::store(url, resp);
    return resp;
}

// ═══════════════════════════════════════════════════════════
// detail_socket  –  one-shot HTTP exchange over a BSD socket
// ═══════════════════════════════════════════════════════════

net::HttpResponse
net::HttpClient::detail_socket(const net::HttpRequest &req,
                               bool           include_body,
                               int            maxRedirects,
                               std::optional<std::chrono::seconds> timeout)
{
    net::UrlParser parser(req.url());
    if (!parser.is_valid())
        return net::HttpResponse{0, "Invalid URL", net::HeaderList{}, {}};
    if (parser.scheme() != net::Scheme::Http)
        return net::HttpResponse{501,
            "Only HTTP supported by BSD-socket backend",
            net::HeaderList{}, {}};

    int sock = connect_socket(parser.host().c_str(), parser.port());
    if (sock < 0)
        return net::HttpResponse{0, "connect() failed", net::HeaderList{}, {}};

    std::string reqStr = build_request_string(parser, req, include_body);
    std::string raw    = send_and_receive(sock, reqStr);

    RawParts parts = split_headers_body(raw);
    if (parts.headers_raw.empty())
        return net::HttpResponse{0, "Malformed HTTP response",
                                 net::HeaderList{}, {}};

    // ── parse status line ───────────────────────────────────
    auto sl = parse_status_line(parts.headers_raw);

    // ── parse headers ───────────────────────────────────────
    net::HeaderList headers;
    {
        size_t pos = parts.headers_raw.find("\r\n");
        if (pos == std::string::npos)
            pos = parts.headers_raw.find("\n");
        pos = (pos == std::string::npos) ? 0 : pos + 1;
        while (pos < parts.headers_raw.size()) {
            size_t lf  = parts.headers_raw.find('\n', pos);
            size_t end = (lf == std::string::npos)
                       ? parts.headers_raw.size() : lf;
            std::string_view line{
                parts.headers_raw.data() + pos, end - pos};
            size_t next = (lf == std::string::npos)
                        ? parts.headers_raw.size() : lf + 1;
            // Skip blank separator line
            if (line.empty() || line[0] == '\r' || line[0] == '\n') {
                pos = next;
                continue;
            }
            size_t colon = line.find(':');
            if (colon != std::string_view::npos) {
                std::string key = std::string{line.substr(0, colon)};
                std::string val = std::string{line.substr(colon + 1)};
                // Strip leading whitespace from value
                size_t vp = 0;
                while (vp < val.size() &&
                       (val[vp] == ' ' || val[vp] == '\t')) ++vp;
                val.erase(0, vp);
                headers.add(std::move(key), std::move(val));
            }
            pos = next;
        }
    }

    // ── body + decompression ────────────────────────────────
    std::string body = include_body
                     ? decompress_body(parts.body, headers) : std::string{};

    // ── redirect follow ─────────────────────────────────────
    if (is_redirect(net::HttpStatus{sl.code}) && maxRedirects > 0) {
        auto loc = headers.get("location");
        if (loc && !loc->empty()) {
            std::string newUrl;
            if ((*loc)[0] == '/') {
                net::UrlParser origP(req.url());
                if (origP.is_valid()) newUrl = origP.origin() + *loc;
            } else if (loc->rfind("http", 0) == 0 ||
                       loc->rfind("https", 0) == 0) {
                newUrl = *loc;
            } else {
                net::UrlParser origP(req.url());
                if (origP.is_valid())
                    newUrl = origP.origin() + "/" + *loc;
            }
            if (!newUrl.empty()) {
                net::HttpRequest nr{"GET", newUrl};
                nr.set_user_agent(req.user_agent());
                return detail_socket(nr, include_body,
                                     maxRedirects - 1, timeout);
            }
        }
    }

    return net::HttpResponse{sl.code, std::move(sl.text),
                             std::move(headers), std::move(body)};
}

// ═══════════════════════════════════════════════════════════
// Decompression
// ═══════════════════════════════════════════════════════════

std::string
net::HttpClient::decompress_body(const std::string &body,
                                 const net::HeaderList   &headers)
{
    if (auto ce = headers.get("content-encoding")) {
        std::string ce_lower = *ce;
        for (auto &ch : ce_lower) ch = static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch)));
        if (ce_lower.find("gzip") != std::string::npos)
            return decompress_gzip(body);
        if (ce_lower.find("deflate") != std::string::npos)
            return decompress_deflate(body);
    }
    return body;
}

std::string net::HttpClient::decompress_gzip(const std::string &data) {
    return inflate_gzip(data);
}

std::string net::HttpClient::decompress_deflate(const std::string &data) {
    return inflate_deflate_raw(data);
}

// ═══════════════════════════════════════════════════════════
// HttpCache
// ═══════════════════════════════════════════════════════════

net::HttpCache::StoreType &net::HttpCache::_store() {
    static StoreType store;
    return store;
}

net::CacheEntry
net::HttpCache::_parse_cache_entry(const net::HttpResponse &resp)
{
    CacheEntry entry{};

    // Cache-Control
    if (auto cc = resp.headers().get("cache-control")) {
        std::string lowered = *cc;
        for (auto &ch : lowered) ch = static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch)));
        if (lowered.find("no-store")        != std::string::npos) entry.no_store         = true;
        if (lowered.find("no-cache")        != std::string::npos) entry.no_cache         = true;
        if (lowered.find("must-revalidate") != std::string::npos) entry.must_revalidate = true;

        size_t ma = lowered.find("max-age=");
        if (ma != std::string::npos) {
            ma += 8;
            long v = 0;
            try { v = std::stol(cc->substr(ma)); } catch (...) {}
            if (v >= 0) entry.max_age_seconds = v;
        }
    }

    // ETag
    if (auto et = resp.headers().get("etag"))
        entry.etag = *et;

    // Last-Modified
    if (auto lm = resp.headers().get("last-modified"))
        entry.last_modified = *lm;

    // Expires
    if (auto exp = resp.headers().get("expires"))
        entry.expires = resp.parse_date(*exp);

    return entry;
}

std::optional<net::CacheEntry>
net::HttpCache::lookup(const std::string &url,
                       net::HttpRequest         *req,
                       bool                     *has_conditional_hdrs,
                       long                     *fresh_outcome)
{
    (void)req; (void)has_conditional_hdrs; (void)fresh_outcome;

    auto &store = _store();
    auto it = store.find(url);
    if (it == store.end()) return std::nullopt;

    CacheEntry &entry = it->second;
    if (entry.no_store) { store.erase(it); return std::nullopt; }

    using clock = std::chrono::system_clock;
    using secs  = std::chrono::seconds;
    bool expired = false;
    long remaining = -1;

    if (entry.max_age_seconds >= 0) {
        if (!entry._stored_at.time_since_epoch().count())
            entry._stored_at = clock::now();
        long age = static_cast<long>(
            std::chrono::duration_cast<secs>(
                clock::now() - entry._stored_at).count());
        remaining = entry.max_age_seconds - age;
        if (remaining <= 0) expired = true;
    } else if (entry.expires.time_since_epoch().count() == 0) {
        expired = true;
    } else {
        if (clock::now() > entry.expires) expired = true;
    }

    if (fresh_outcome) *fresh_outcome = remaining;

    if (expired && req) {
        if (!entry.etag.empty()) {
            req->add_header("If-None-Match", entry.etag);
            if (has_conditional_hdrs) *has_conditional_hdrs = true;
        }
        if (!entry.last_modified.empty()) {
            req->add_header("If-Modified-Since", entry.last_modified);
            if (has_conditional_hdrs) *has_conditional_hdrs = true;
        }
    }

    return expired ? std::nullopt : std::make_optional(entry);
}

void net::HttpCache::store(const std::string &url, const net::HttpResponse &resp) {
    if (!resp.ok())
        return;

    CacheEntry entry = _parse_cache_entry(resp);
    if (entry.no_store)
        return;

    if (entry.max_age_seconds < 0 &&
        entry.expires.time_since_epoch().count() == 0)
        return; // no freshness signal

    entry.cached_body  = resp.body();
    entry._stored_at   = std::chrono::system_clock::now();

    auto &store = _store();
    if (store.size() >= static_cast<size_t>(kMaxEntries)) {
        size_t minBytes = SIZE_MAX;
        auto   oldest_it = store.begin();
        for (auto it = store.begin(); it != store.end(); ++it) {
            size_t bs = it->second.cached_body.size();
            if (bs < minBytes) {
                minBytes  = bs;
                oldest_it = it;
            }
        }
        if (oldest_it != store.end()) store.erase(oldest_it);
    }
    store[url] = std::move(entry);
}

void net::HttpCache::evict(const std::string &url) {
    _store().erase(url);
}

void net::HttpCache::clear() {
    _store().clear();
}

size_t net::HttpCache::total_bytes() {
    size_t total = 0;
    for (auto &[k, e] : _store())
        total += e.cached_body.size();
    return total;
}

// ═══════════════════════════════════════════════════════════
// HttpRequest – cookies / default headers
// ═══════════════════════════════════════════════════════════

void net::HttpRequest::set_cookies(const std::vector<std::string> &cookies) {
    for (auto &c : cookies)
        add_header("Cookie", c);
}

void net::HttpRequest::add_default_headers(const std::string &referer) {
    if (!header("User-Agent").has_value())
        set_header("User-Agent", "NitrogenBrowser/0.1");
    if (!header("Accept").has_value())
        set_header("Accept",
                   "text/html,application/xhtml+xml,*/*;q=0.1");
    if (!header("Accept-Language").has_value())
        set_header("Accept-Language", "en-US,en;q=0.5");
    if (!header("Accept-Encoding").has_value())
        set_header("Accept-Encoding", "gzip, deflate, br");
    if (!referer.empty() && !header("Referer").has_value())
        set_header("Referer", referer);
    if (!header("Connection").has_value())
        set_header("Connection", "keep-alive");
}

// ═══════════════════════════════════════════════════════════
// CookieManager
// ═══════════════════════════════════════════════════════════

net::CookieManager::JarType &net::CookieManager::_jar() {
    static JarType jar;
    return jar;
}

bool net::CookieManager::_path_match(std::string_view req_path,
                                      std::string_view cookie_path)
{
    // Normalise leading slashes.
    while (!req_path.empty()  && req_path[0]       != '/') req_path.remove_prefix(1);
    while (!cookie_path.empty() && cookie_path[0]  != '/') cookie_path.remove_prefix(1);
    if (cookie_path.empty()) return true;
    return req_path.size() >= cookie_path.size()
        && req_path.substr(0, cookie_path.size()) == cookie_path;
}

bool net::CookieManager::_domain_match(std::string_view req_host,
                                        std::string_view ck_domain)
{
    if (req_host == ck_domain) return true;
    if (!ck_domain.empty() && ck_domain[0] == '.') {
        std::string base = std::string{ck_domain}.substr(1);
        if (req_host.size() > base.size())
            return req_host.compare(req_host.size() - base.size(), base.size(), base) == 0;
    }
    return false;
}

bool net::CookieManager::_expired(const Cookie &ck) {
    auto ts = ck.expires.time_since_epoch().count();
    if (ts == 0) return false; // session cookie
    return std::chrono::system_clock::now() > ck.expires;
}

std::chrono::system_clock::time_point
net::CookieManager::_parse_cookie_date(std::string_view raw) {
    int day, year, hour, min, sec;
    char monStr[4]{};
    std::tm tm = parse_rfcdate_parts(raw, day, year, hour, min, sec, monStr);
    if (day == 0 && year == 0)
        return std::chrono::system_clock::time_point{};
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

net::Cookie
net::CookieManager::parse_set_cookie(std::string_view raw,
                                     const std::string & /*response_url*/)
{
    Cookie ck;

    auto trim = [](std::string_view sv) {
        while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front())))
            sv.remove_prefix(1);
        while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())))
            sv.remove_suffix(1);
        return sv;
    };

    // name=value comes first (before any ';')
    size_t semi = raw.find(';');
    std::string_view nameval = semi != std::string_view::npos
                             ? raw.substr(0, semi) : raw;
    nameval = trim(nameval);

    size_t eq = nameval.find('=');
    if (eq != std::string_view::npos) {
        ck.name  = std::string{trim(nameval.substr(0, eq))};
        ck.value = std::string{trim(nameval.substr(eq + 1))};
    }

    // Attributes after the first ';'
    size_t pos = semi != std::string_view::npos ? semi + 1
               : std::string_view::npos;
    while (pos < raw.size()) {
        size_t next = raw.find(';', pos);
        std::string_view tok = trim(
            next != std::string_view::npos ? raw.substr(pos, next - pos)
                                           : raw.substr(pos));
        pos = (next == std::string_view::npos) ? raw.size() : next + 1;

        size_t aeq = tok.find('=');
        std::string_view key = (aeq != std::string_view::npos)
                               ? tok.substr(0, aeq) : tok;
        std::string_view val = (aeq != std::string_view::npos)
                               ? tok.substr(aeq + 1) : std::string_view{};

        std::string lk;
        lk.reserve(key.size());
        for (char ch : key) lk.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch))));

        if (lk == "expires") {
            ck.expires = _parse_cookie_date(val);
        } else if (lk == "max-age") {
            try { ck.expires = std::chrono::system_clock::now()
                              + std::chrono::seconds(
                                  std::stol(std::string{val})); }
            catch (...) {}
        } else if (lk == "domain") {
            ck.domain = std::string{val};
        } else if (lk == "path") {
            ck.path = std::string{val};
        } else if (lk == "secure") {
            ck.secure = true;
        } else if (lk == "httponly") {
            ck.http_only = true;
        }
    }

    return ck;
}

void net::CookieManager::set_cookie(const Cookie &ck) {
    auto &jar = _jar();
    std::string key = ck.domain + "|" + ck.path;

    // Purge expired entries on this key.
    auto &vec = jar[key];
    vec.erase(std::remove_if(vec.begin(), vec.end(),
                [](const Cookie &e){ return _expired(e); }), vec.end());

    // Update existing cookie of same name, else append.
    for (auto &e : vec) {
        if (e.name == ck.name) { e = ck; return; }
    }
    vec.push_back(ck);
}

void net::CookieManager::set_cookies_from_response(
        const std::string             &url,
        const net::HeaderList         &hdrs)
{
    std::vector<std::string_view> all;
    {
        auto a = hdrs.get_all("set-cookie");
        auto b = hdrs.get_all("Set-Cookie");
        all.insert(all.end(), a.begin(), a.end());
        all.insert(all.end(), b.begin(), b.end());
    }
    if (all.empty()) return;

    net::UrlParser parser(url);
    std::string host = parser.is_valid() ? parser.host() : std::string{};

    for (auto &raw : all) {
        Cookie ck = parse_set_cookie(raw, url);
        if (ck.domain.empty() && !host.empty()) ck.domain = host;
        if (ck.secure && parser.is_valid()
            && parser.scheme() != net::Scheme::Https) continue;
        set_cookie(ck);
    }
}

std::vector<std::string> net::CookieManager::get_cookies(const std::string &url) {
    std::vector<std::string> out;
    net::UrlParser parser(url);
    if (!parser.is_valid()) return out;

    std::string host = parser.host();
    std::string path = parser.path();
    bool isHttps = parser.scheme() == net::Scheme::Https;

    for (auto &[key, vec] : _jar()) {
        (void)key;
        for (auto &ck : vec) {
            if (_expired(ck)) continue;
            if (ck.secure && !isHttps) continue;
            if (!_domain_match(host, ck.domain)) continue;
            if (!_path_match(path, ck.path))     continue;
            if (!ck.name.empty())
                out.push_back(ck.name + "=" + ck.value);
        }
    }
    return out;
}

std::string net::CookieManager::cookie_header(const std::string &url) {
    auto cookies = get_cookies(url);
    std::string combined;
    for (size_t i = 0; i < cookies.size(); ++i) {
        if (i > 0) combined += "; ";
        combined += cookies[i];
    }
    return combined;
}

size_t net::CookieManager::purge_expired() {
    auto &jar = _jar();
    size_t removed = 0;
    for (auto &[key, vec] : jar) {
        size_t before = vec.size();
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                    [](const Cookie &ck){ return _expired(ck); }),
                  vec.end());
        removed += before - vec.size();
    }
    for (auto it = jar.begin(); it != jar.end(); )
        it = (it->second.empty()) ? jar.erase(it) : ++it;
    return removed;
}

void net::CookieManager::clear() {
    _jar().clear();
}

size_t net::CookieManager::size() {
    size_t n = 0;
    for (auto &[k, vec] : _jar()) n += vec.size();
    return n;
}

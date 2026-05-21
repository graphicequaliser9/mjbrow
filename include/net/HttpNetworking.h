#ifndef NITROGEN_HTTP_NETWORKING_H
#define NITROGEN_HTTP_NETWORKING_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <algorithm>
#include <cctype>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

namespace net {

// forward declaration (HttpClient appears at end of file)
class HttpClient;

// ═══════════════════════════════════════════════════════════
// enums
// ═══════════════════════════════════════════════════════════

enum class Scheme {
    Http, Https, File, Invalid
};

enum class HttpStatus : int {
    Continue                         = 100,
    SwitchingProtocols               = 101,
    OK                               = 200,
    Created                          = 201,
    Accepted                         = 202,
    NonAuthoritativeInfo             = 203,
    NoContent                        = 204,
    ResetContent                     = 205,
    PartialContent                   = 206,
    MultipleChoices                  = 300,
    MovedPermanently                 = 301,
    Found                            = 302,
    SeeOther                         = 303,
    NotModified                      = 304,
    UseProxy                         = 305,
    TemporaryRedirect                = 307,
    PermanentRedirect                = 308,
    BadRequest                       = 400,
    Unauthorized                     = 401,
    PaymentRequired                  = 402,
    Forbidden                        = 403,
    NotFound                         = 404,
    MethodNotAllowed                 = 405,
    NotAcceptable                    = 406,
    ProxyAuthRequired                = 407,
    RequestTimeout                   = 408,
    Conflict                         = 409,
    Gone                             = 410,
    LengthRequired                   = 411,
    PreconditionFailed               = 412,
    RequestEntityTooLarge            = 413,
    RequestUriTooLong                = 414,
    UnsupportedMediaType             = 415,
    RequestedRangeNotSatisfiable     = 416,
    ExpectationFailed                = 417,
    ImATeapot                        = 418,
    MisdirectedRequest               = 421,
    UnprocessableEntity              = 422,
    Locked                           = 423,
    FailedDependency                 = 424,
    UpgradeRequired                  = 426,
    TooManyRequests                  = 429,
    InternalServerError              = 500,
    NotImplemented                   = 501,
    BadGateway                       = 502,
    ServiceUnavailable               = 503,
    GatewayTimeout                   = 504,
    HttpVersionNotSupported          = 505,
};

inline bool is_success(HttpStatus s) {
    return static_cast<int>(s) >= 200 && static_cast<int>(s) < 300;
}
inline bool is_redirect(HttpStatus s) {
    int c = static_cast<int>(s);
    return c == 301 || c == 302 || c == 303 || c == 307 || c == 308;
}

inline Scheme scheme_from_string(std::string_view sv) {
    if (sv.size() < 4) return Scheme::Invalid;
    char c4[5]{};
    std::copy_n(sv.data(), 4, c4); c4[4] = 0;
    for (auto &ch : c4) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    bool isHttp4 = std::string_view{"http"} == c4;
    bool isHttps5 = false;
    if (sv.size() >= 5) {
        char c5[6]{};
        std::copy_n(sv.data(), 5, c5); c5[5] = 0;
        for (auto &ch : c5) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        isHttps5 = std::string_view{"https"} == c5;
    }
    if (isHttps5) return Scheme::Https;
    if (isHttp4) return Scheme::Http;
    return Scheme::Invalid;
}

inline const char *scheme_to_string(Scheme s) {
    switch (s) {
        case Scheme::Http:  return "http";
        case Scheme::Https: return "https";
        case Scheme::File:  return "file";
        default:            return "invalid";
    }
}

// ═══════════════════════════════════════════════════════════
// Case-insensitive hash and equality (for HTTP header names)
// ═══════════════════════════════════════════════════════════

struct CaseInsensitiveHash {
    size_t operator()(std::string_view sv) const noexcept {
        size_t h = 5381;
        for (unsigned char c : sv) h = ((h << 5) + h) + static_cast<size_t>(std::tolower(c));
        return h;
    }
};

struct CaseInsensitiveEq {
    bool operator()(std::string_view a, std::string_view b) const noexcept {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        return true;
    }
};

using HeaderMap = std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEq>;

// ═══════════════════════════════════════════════════════════
// HeaderList
// ───────────────────────────────────────────────────────────
// Design:
//   _entries is the authoritative store, always in insertion order.
//   _indices (unordered_multimap) records key → latest _entries index.
//
//   add(key, val):
//     New key      → append new row, record its index.
//     Duplicate key→ move existing _entries[row] to end, re-index to latest.
//     get(key)     → same row as _indices returns for this key.
//     to_map()     → last row wins per key (same as get()).
// ───────────────────────────────────────────────────────────
//
//  Quick invariant:
//    after `add(k,v1); add(k,v2)`:
//      _entries has 1 row: (key, v2)
//      _indices has 1 entry: k → 0
//      get(k) / to_map()[k] both return v2
// ───────────────────────────────────────────────────────────

class HeaderList {
public:
    void add(std::string key, std::string value) {
        auto lowered = [](std::string k) {
            std::string r; r.reserve(k.size());
            for (char ch : k)
                r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            return r;
        }(key);

        auto it = _indices.find(lowered);
        if (it != _indices.end()) {
            // Duplicate key:
            //   1. erase the stale index  → bucket head is now the next-oldest (or empty)
            //   2. emplace_back a fresh _entries row
            //   3. emplace the new index at the head of the bucket chain
            // equal_range always starts from the head, so range.first lands on
            // the newest index  → get() / to_map() both see the latest value.
            _indices.erase(it);
            size_t new_idx = _entries.size();
            _entries.emplace_back(std::move(key), std::move(value));
            _indices.emplace(lowered, static_cast<int>(new_idx));
        } else {
            // Fresh key: append new row, record its index.
            int idx = static_cast<int>(_entries.size());
            _entries.emplace_back(std::move(key), std::move(value));
            _indices.emplace(std::move(lowered), idx);
        }
    }

    void set(std::string key, std::string value) {
        auto lowered = [](std::string k) {
            std::string r; r.reserve(k.size());
            for (char ch : k)
                r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            return r;
        }(key);

        auto range = _indices.equal_range(lowered);
        _indices.erase(range.first, range.second);
        _entries.emplace_back(std::move(key), std::move(value));
        _indices.emplace(std::move(lowered),
                         static_cast<int>(_entries.size()) - 1);
    }

    std::optional<std::string> get(std::string_view key) const {
        auto lowered = [](std::string_view sv) {
            std::string r; r.reserve(sv.size());
            for (char ch : sv)
                r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            return r;
        }(key);

        // equal_range returns all active indices in insertion order.
        // Take the FIRST one: that is always the EARLIEST "current" record
        // for this key (established by set() / the original add()) and is
        // NOT affected by the stale index that add() left behind after
        // erase+reinsert.
        auto range = _indices.equal_range(lowered);
        if (range.first == range.second) return std::nullopt;
        return _entries[range.first->second].second;
    }

    std::vector<std::string_view> get_all(std::string_view key) const {
        auto lowered = [](std::string_view sv) {
            std::string r; r.reserve(sv.size());
            for (char ch : sv)
                r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            return r;
        }(key);

        auto range = _indices.equal_range(lowered);
        std::vector<std::string_view> out;
        out.reserve(static_cast<size_t>(std::distance(range.first, range.second)));
        for (auto it = range.first; it != range.second; ++it)
            out.push_back(std::string_view{_entries[it->second].second});
        return out;
    }

    void remove(std::string_view key) {
        auto lowered = [](std::string_view sv) {
            std::string r; r.reserve(sv.size());
            for (char ch : sv)
                r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            return r;
        }(key);
        auto range = _indices.equal_range(lowered);
        _indices.erase(range.first, range.second);
    }

    bool has(std::string_view key) const {
        auto lowered = [](std::string_view sv) {
            std::string r; r.reserve(sv.size());
            for (char ch : sv)
                r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            return r;
        }(key);
        return _indices.find(lowered) != _indices.end();
    }

    void clear() {
        _entries.clear();
        _indices.clear();
    }

    std::vector<std::pair<std::string, std::string>> &entries()       { return _entries; }
    const std::vector<std::pair<std::string, std::string>> &entries() const { return _entries; }
    size_t size() const { return _entries.size(); }

    HeaderMap to_map() const {
        HeaderMap m;
        for (auto &[k, v] : _entries) m[k] = v;
        return m;
    }

private:
    std::vector<std::pair<std::string, std::string>>               _entries;
    std::unordered_multimap<std::string, int, CaseInsensitiveHash, CaseInsensitiveEq>
                                                                    _indices;
};

// ═══════════════════════════════════════════════════════════
// UrlParser
// ═══════════════════════════════════════════════════════════

class UrlParser {
public:
    explicit inline UrlParser(std::string_view url) : _raw(url) { _parse(); }

    const std::string        &raw()      const { return _raw; }
    Scheme                     scheme()   const { return _scheme; }
    const std::string        &host()     const { return _host; }
    const std::string        &port_str() const { return _port_str; }
    const std::string        &path()     const { return _path; }
    const std::string        &query()    const { return _query; }
    const std::string        &fragment() const { return _fragment; }
    const std::string        &username() const { return _username; }
    const std::string        &password() const { return _password; }
    int                        port()     const { return _port; }
    bool                       has_userinfo() const { return !_username.empty(); }
    bool                       is_valid() const { return _valid; }

    std::string origin() const {
        std::string o = scheme_to_string(_scheme);
        o += "://" + _host;
        if ((_scheme == Scheme::Http  && _port != 80) ||
            (_scheme == Scheme::Https && _port != 443))
            o += ":" + _port_str;
        return o;
    }

    std::string encoded_path_query() const {
        std::string r = _path;
        if (!_query.empty()) { r += "?"; r += _query; }
        return r;
    }

private:
    void _parse();

    std::string              _raw;
    Scheme                   _scheme{Scheme::Invalid};
    std::string              _host;
    std::string              _port_str;
    std::string              _path;
    std::string              _query;
    std::string              _fragment;
    std::string              _username;
    std::string              _password;
    int                      _port{0};
    bool                     _valid{false};
};

// ═══════════════════════════════════════════════════════════
// HttpRequest
// ═══════════════════════════════════════════════════════════

class HttpRequest {
public:
    HttpRequest() = default;
    HttpRequest(std::string method,
                std::string url,
                HeaderList headers = {},
                std::string body = {});

    const std::string      &method()   const { return _method; }
    const std::string      &url()      const { return _url; }
    const HeaderList       &headers()  const { return _headers; }
    const std::string      &body()     const { return _body; }
    const std::string      &user_agent() const { return _user_agent; }

    void set_method(std::string m)         { _method = std::move(m); }
    void set_url(std::string u)            { _url = std::move(u); }
    void set_body(std::string b)           { _body = std::move(b); }
    void set_user_agent(std::string ua)    { _user_agent = std::move(ua); }
    void set_header(std::string k, std::string v) { _headers.set(std::move(k), std::move(v)); }
    void add_header(std::string k, std::string v)  { _headers.add(std::move(k), std::move(v)); }
    void remove_header(std::string_view k)         { _headers.remove(k); }
    void set_cookies(const std::vector<std::string> &cookies);
    void add_default_headers(const std::string &referer = {});

    std::optional<std::string> header(std::string_view key) const { return _headers.get(key); }

    static inline HttpRequest get(std::string url) {
        return HttpRequest{"GET", std::move(url), {}, {}};
    }
    static inline HttpRequest head(std::string url) {
        return HttpRequest{"HEAD", std::move(url), {}, {}};
    }
    static inline HttpRequest post(std::string url, std::string body = {}) {
        return HttpRequest{"POST", std::move(url), {}, std::move(body)};
    }

private:
    std::string         _method{"GET"};
    std::string         _url;
    HeaderList          _headers;
    std::string         _body;
    std::string         _user_agent;
};

// ═══════════════════════════════════════════════════════════
// HttpResponse
// ═══════════════════════════════════════════════════════════

class HttpResponse {
public:
    HttpResponse() = default;
    HttpResponse(int statusCode, std::string statusText,
                 HeaderList headers, std::string body);

    int                 status_code() const { return _statusCode; }
    const std::string  &status_text()  const { return _statusText; }
    const HeaderList   &headers()     const { return _headers; }
    const std::string  &body()         const { return _body; }
    bool                ok()           const { return _statusCode >= 200 && _statusCode < 300; }
    bool                is_redirect()  const {
        int c = _statusCode;
        return c == 301 || c == 302 || c == 303 || c == 307 || c == 308;
    }

    std::optional<std::string> header(std::string_view key) const { return _headers.get(key); }

    std::string content_type() const {
        auto ct = header("content-type");
        if (!ct || ct->empty()) return "text/html; charset=utf-8";
        size_t semi = ct->find(';');
        if (semi != std::string::npos) {
            std::string_view sv{*ct};
            while (semi > 0 && std::isspace(static_cast<unsigned char>(sv[semi - 1]))) --semi;
            return std::string{sv.substr(0, semi)};
        }
        return *ct;
    }

    std::string charset() const {
        auto ct = header("content-type");
        if (!ct) return "utf-8";
        size_t pos = ct->find("charset=");
        if (pos == std::string::npos) return "utf-8";
        pos += 8;
        size_t end = ct->find(';', pos);
        std::string ch = ct->substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        while (!ch.empty() && std::isspace(static_cast<unsigned char>(ch.back())))
            ch.pop_back();
        return ch;
    }

    int64_t content_length() const {
        auto cl = header("content-length");
        if (!cl) return -1;
        try { return std::stoll(*cl); } catch (...) { return -1; }
    }

    std::chrono::system_clock::time_point parse_date(std::string_view value) const;

    void set_status_code(int code) { _statusCode = code; }
    void set_status_text(std::string t) { _statusText = std::move(t); }

private:
    int                 _statusCode{};
    std::string         _statusText;
    HeaderList          _headers;
    std::string         _body;
};

// ═══════════════════════════════════════════════════════════
// CacheEntry – HTTP cache metadata
// ═══════════════════════════════════════════════════════════

struct CacheEntry {
    std::string                            etag;
    std::string                            last_modified;
    std::chrono::system_clock::time_point  expires;
    bool                                   no_store{false};
    bool                                   no_cache{false};
    bool                                   must_revalidate{false};
    long                                   max_age_seconds{-1};
    std::string                            cached_body;
    std::chrono::system_clock::time_point  _stored_at;
};

// ═══════════════════════════════════════════════════════════
// HttpCache – in-memory HTTP response cache
// ═══════════════════════════════════════════════════════════
//
// Driven by CacheEntry / CacheControl / Expires / ETag / Last-Modified.
//
// Ops flow
// ────────
//  lookup(url, req):
//   · find a matching CacheEntry
//   · if it is expired or stale call _make_conditional(req, entry) → add
//     If-None-Match / If-Modified-Since headers
//   · return { found, entry } so the caller can short-circuit with 304
//
//  store(url, resp):
//   · parse Cache-Control / Age / Expires / ETag / Last-Modified from resp
//   · update or create CacheEntry
//   · save a copy of the body

class HttpCache {
public:
    // Find a cached entry for url.
    //   req                     – request object; conditional headers
    //                             (If-None-Match / If-Modified-Since) are
    //                             added here when a stale entry is found.
    //   *has_conditional_hdrs  – whether conditional headers were injected
    //   *fresh_outcome         – seconds of freshness remaining (-1 = stale)
    static std::optional<CacheEntry>
    lookup(const std::string &url,
           HttpRequest       *req,
           bool              *has_conditional_hdrs,
           long              *fresh_outcome);

    // Store a response body in the cache, keyed by url.
    static void store(const std::string &url, const HttpResponse &resp);

    // Evict a single cache entry for url.
    static void evict(const std::string &url);

    // Clear all cache entries.
    static void clear();

    // Estimated cache size across all stored bodies.
    static size_t total_bytes();

    static constexpr int kMaxEntries = 512;

    // Small helper returned by parse_status_line used by HttpCache and detail_socket.
    struct StatusLineResult {
        int         code{200};
        std::string text{"OK"};
    };

private:
    // Map of normalised URL → CacheEntry (one per URL).
    // We purposefully do NOT expose public mutation: all goes through
    // lookup + store which own the parsing of cache headers.
    using StoreType = std::unordered_map<std::string, CacheEntry>;
    static StoreType &_store();

    // Parse Cache-Control / Age / Expires / ETag / Last-Modified from response
    // headers into a CacheEntry struct.
    static CacheEntry _parse_cache_entry(const HttpResponse &resp);
};

// ═══════════════════════════════════════════════════════════
// Cookie – a single RFC-6265 cookie
// ═══════════════════════════════════════════════════════════

struct Cookie {
    std::string name;
    std::string value;
    std::string domain;
    std::string path{"/"};
    bool        secure{false};
    bool        http_only{false};
    // 0 == session cookie (no expires / max-age)
    std::chrono::system_clock::time_point expires;
};

// ═══════════════════════════════════════════════════════════
// CookieManager – RFC-6265 cookie jar
// ═══════════════════════════════════════════════════════════
//
// Responsibilities
// ─────────────────
//  · parse_set_cookie  – parse a Set-Cookie header value into a Cookie
//  · set_cookie        – store / update a cookie for its domain + path
//  · get_cookies       – return cookies matching a request URL (scheme + domain + path)
//  · clear             – wipe all stored cookies
//
// Matching rules (RFC 6265)
// ──────────────────────────
//  · domain-match: request host must domain-match cookie domain
//  · path-match:   request path must be a prefix of cookie path (or equal)

class CookieManager {
public:
    // Parse a raw Set-Cookie header value into a Cookie struct.
    static Cookie parse_set_cookie(std::string_view raw,
                                   const std::string &response_url);

    // Store (or update) a cookie.
    static void set_cookie(const Cookie &ck);

    // Set a cookies from parsed Set-Cookie response for a URL.
    static void set_cookies_from_response(const std::string &url,
                                          const HeaderList &headers);

    // Return cookies that should be sent with a request to the given URL.
    // Cookie names will have their value coerced to string for the output.
    // Values are NOT automatically quoted – callers include them directly.
    static std::vector<std::string> get_cookies(const std::string &url);

    // Serialise stored cookies for a URL as a single Cookie: header value.
    static std::string cookie_header(const std::string &url);

    // Remove expired cookies and return count removed.
    static size_t purge_expired();

    // Clear the jar entirely.
    static void clear();

    // Total cookies currently stored.
    static size_t size();

private:
    using JarType = std::unordered_map<std::string, std::vector<Cookie>>;
    static JarType &_jar();
    static bool _path_match(std::string_view req_path, std::string_view cookie_path);
    static bool _domain_match(std::string_view req_host, std::string_view cookie_domain);
    static bool _expired(const Cookie &ck);
    static std::chrono::system_clock::time_point
    _parse_cookie_date(std::string_view raw);
};

// ═══════════════════════════════════════════════════════════
// HttpClient
// ───────────────────────────────────────────────────────────
// WinHTTP on Windows; BSD sockets (POSIX) as fallback.
// ═══════════════════════════════════════════════════════════

class HttpClient final {
public:
    HttpClient() = default;
    ~HttpClient() = default;

    // Call once at program start (no-op on non-Windows)
    static void init();
    static void shutdown();

    // GET – follows up to maxRedirects (default 5)
    static HttpResponse get(const HttpRequest &req,
                            int maxRedirects = 5,
                            std::optional<std::chrono::seconds> timeout = {});

    // HEAD – fetches headers only (no body)
    static HttpResponse head(const HttpRequest &req,
                             int maxRedirects = 5,
                             std::optional<std::chrono::seconds> timeout = {});

    // GET with explicit cache window:
    //   stale_after  – response is considered stale after this many seconds
    //   use_cache    – if false, bypasses cache entirely
    // Returns cached body on 304 Not Modified, otherwise fetches from network.
    static HttpResponse get_cached(const HttpRequest &req,
                                   std::chrono::seconds stale_after,
                                   bool use_cache = true,
                                   std::optional<std::chrono::seconds> timeout = {});

    // POST – does not follow redirects by default
    static HttpResponse post(const HttpRequest &req,
                             int maxRedirects = 0,
                             std::optional<std::chrono::seconds> timeout = {});

private:
    friend class HttpCache;

    // Low-level socket exchange.
    // include_body=false skips body read (used for HEAD).
    static HttpResponse detail_socket(const HttpRequest &req,
                                      bool            include_body,
                                      int             maxRedirects,
                                      std::optional<std::chrono::seconds> timeout);

    // Per-response gzip / deflate decompression.
    static std::string decompress_body(const std::string &body,
                                       const HeaderList &headers);
    static std::string decompress_gzip(const std::string &data);
    static std::string decompress_deflate(const std::string &data);
};

} // namespace net

#endif // NITROGEN_HTTP_NETWORKING_H

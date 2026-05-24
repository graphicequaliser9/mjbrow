/**
 * @file URLBar.cpp
 * @brief URL bar implementation: autocomplete, Google Suggest fetch, navigation.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "browser/URLBar.h"
#include "net/HttpClient.h"
#include "util/Logging.h"
#include "util/String.h"

#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace browser {

URLBar::URLBar() = default;
URLBar::~URLBar() = default;

void URLBar::setCurrentUrl(const std::string& url) {
    committedUrl_ = url;
    inputText_   = url;
}

std::string URLBar::committedUrl() const {
    return committedUrl_.empty() ? inputText_ : committedUrl_;
}

void URLBar::setFocus() {
    focused_ = true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"

// ── Google Suggest JSONP parser ──────────────────────────────────────────
//
// The API returns a JSON array of arrays:
//   ["query", ["completion 1","completion 2",...], [...], ...]
// We only care about element [1] (the top suggestion strings).
//
std::vector<SuggestionItem> URLBar::parseGoogleSuggest(const std::string& raw) {
    std::vector<SuggestionItem> out;

    // Find the second array element: start at '[' then balance brackets
    auto firstBracket = raw.find('[');
    if (firstBracket == std::string::npos) return out;

    size_t pos = firstBracket + 1;

    // Skip first element (the echoed query string)
    size_t depth = 0;
    bool inStr = false;
    char prev = 0;
    while (pos < raw.size()) {
        char c = raw[pos];
        if (!inStr) {
            if (c == '"') inStr = true;
            else if (c == '[') ++depth;
            else if (c == ']') {
                if (depth <= 0) break;
                --depth;
            }
        } else {
            if (c == '"' && prev != '\\') inStr = false;
        }
        prev = c;
        ++pos;
    }
    // pos now sits at the ']' closing the first element; skip ", " to second element
    while (pos < raw.size() && (raw[pos] == ',' || raw[pos] == ' ' || raw[pos] == '\t'))
        ++pos;

    // Read the second element (array of suggestion strings)
    if (pos < raw.size() && raw[pos] == '[') {
        inStr  = false;
        prev   = 0;
        depth  = 0;
        ++pos;
        while (pos < raw.size() && out.size() < 5) {
            char c = raw[pos];
            if (!inStr) {
                if (c == '"') {
                    inStr = true;
                    std::string token;
                    ++pos;
                    while (pos < raw.size()) {
                        char tc = raw[pos];
                        if (tc == '"' && prev != '\\') break;
                        if (tc == '\\' && prev == '\\') {
                            token.pop_back();  // escaped backslash
                        }
                        token += tc;
                        prev = tc;
                        ++pos;
                    }
                    SuggestionItem item;
                    item.text = token;
                    // The second JSON array element is completion strings;
                    // no separate URL is returned, so URL = "https://www.google.com/search?q=..."
                    item.url = "https://www.google.com/search?q=" +
                               std::string(token.begin(), std::remove_if(token.begin(), token.end(), [](unsigned char c) {
                                   return c == ' ';
                               }));
                    if (!token.empty()) out.push_back(item);
                } else if (c == ']') {
                    break;
                } else if (c == ',') {
                    // separator between elements
                }
            }
            // end token extraction
            if (inStr) {
                prev = c;
                ++pos;
                continue;
            }
            ++pos;
        }
    }

    return out;
}

#pragma GCC diagnostic pop

void URLBar::onKeyDown(char ch) {
    if (!focused_) return;
    if (ch >= 32 && ch <= 126) {
        inputText_ += ch;
        // Schedule debounced fetch
        fetchPending_ = true;
    }
}

void URLBar::onSpecialKey(int keyCode) {
    if (!focused_) return;

    switch (keyCode) {
    case 0x0D:  // VK_RETURN / Enter
        if (navigateCb_) navigateCb_(inputText_.empty() ? committedUrl_ : inputText_);
        committedUrl_ = inputText_.empty() ? committedUrl_ : inputText_;
        inputText_    = committedUrl_;
        suggestions_.clear();
        highlighted_  = -1;
        break;

    case 0x1B:  // VK_ESCAPE
        if (!suggestions_.empty()) {
            suggestions_.clear();
        } else {
            inputText_   = committedUrl_;
            highlighted_ = -1;
        }
        break;

    case 0x26:  // VK_UP
        if (!suggestions_.empty()) {
            highlighted_ = std::max(0, highlighted_ - 1);
            if (selectionCb_) selectionCb_(highlighted_);
        }
        break;

    case 0x28:  // VK_DOWN
        if (!suggestions_.empty()) {
            highlighted_ = std::min<int>((int)suggestions_.size() - 1, highlighted_ + 1);
            if (selectionCb_) selectionCb_(highlighted_);
        }
        break;
    }
}

// ── network fetch (runs on a spawned thread; called by platform timer) ──────

void URLBar::fetchSuggestions(const std::string& query) {
    fetchPending_ = false;

    if (query.empty()) {
        suggestions_.clear();
        return;
    }

    // Build URL – lower-case the scheme for case-insensitive hosts,
    // leave the path / query intact (Google Suggest accepts raw query strings).
    std::string url = "https://suggestqueries.google.com/complete/search?client=firefox&q=";

    // Build the encoding into a local buffer to keep -Wunused-but-set quiet
    {
        std::string encoded;
        encoded.reserve(query.size() * 3);
        for (unsigned char c : query) {
            if (c == ' ')
                encoded += '+';
            else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                     || c == '-' || c == '_' || c == '.' || c == ',')
                encoded += (char)c;
            else if (c >= 0x80) {
                char buf[5];
                std::snprintf(buf, sizeof(buf), "%%%02X", c);
                encoded += buf;
            }
        }
        url += encoded;
    }

    net::HttpClient client;
    auto response = client.sendRequest(url);
    std::string body(response.body.begin(), response.body.end());

    if (body.empty()) {
        util::Log(util::LogLevel::Warn, "URLBar: empty suggest response for query '" + query + "'\n");
        suggestions_.clear();
        return;
    }

    suggestions_ = parseGoogleSuggest(body);
    if (suggestions_.size() > 5) suggestions_.resize(5);

    if (selectionCb_) selectionCb_(0);
    util::Log(util::LogLevel::Info,
              "URLBar: " + std::to_string(suggestions_.size()) + " suggestions for '" + query + "'\n");
}

void URLBar::showSuggestions(const std::vector<SuggestionItem>& items) {
    suggestions_  = items;
    suggestions_.resize(std::min<size_t>(5, suggestions_.size()));
    highlighted_  = -1;
    if (!suggestions_.empty() && selectionCb_) selectionCb_(0);
}

} // namespace browser

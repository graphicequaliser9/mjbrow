/**
 * @file Tab.cpp
 * @brief Tab implementation: navigation, frame tick, paint/layout/JS stub pipeline.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "browser/Tab.h"
#include "browser/SettingsPanel.h"
#include "browser/URLBar.h"

#include "html/HTMLParser.h"
#include "html/DOMNode.h"
#include "net/HttpClient.h"
#include "js/VM.h"
#include "css/CSSParser.h"
#include "util/Logging.h"
#include "util/String.h"

#include <algorithm>
#include <chrono>

namespace browser {

Tab::Tab() = default;

Tab::Tab(const std::string& initialUrl) {
    if (!initialUrl.empty()) navigate(initialUrl);
}

Tab::~Tab() = default;

static std::string extractTextContent(html::DOMNode* node) {
    std::string result;
    if (!node) return result;
    for (html::DOMNode* child = node->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Text) {
            result += child->textContent;
        } else if (child->nodeType == html::NodeType::Element) {
            result += extractTextContent(child);
        }
    }
    return result;
}

void Tab::navigate(const std::string& url) {
    url_     = url;
    loading_ = true;

    util::Log(util::LogLevel::Info, "Tab::navigate → " + url + "\n");

    net::HttpClient client;
    auto response   = client.sendRequest(url, "GET");
    std::string body(response.body.begin(), response.body.end());
    rawHtml_ = body;

    if (rawHtml_.empty()) {
        util::Log(util::LogLevel::Warn, "Tab: empty response from " + url + "\n");
        bodyText_ = "(failed to load content)";
        loading_ = false;
        return;
    }

    parseHTML(rawHtml_);

    html::DOMNode* bodyNode = nullptr;
    if (document_) {
        for (html::DOMNode* child = document_->firstChild; child; child = child->nextSibling) {
            if (child->nodeType == html::NodeType::Element && child->tagName == "body") {
                bodyNode = child;
                break;
            }
        }
    }
    bodyText_ = extractTextContent(bodyNode);

    cascadeStyles();
    performLayout();
    loading_ = false;
}

void Tab::tick(double dtMs) {
    if (loading_ || !document_) return;

    // JS VM tick (requestAnimationFrame injection point in a full engine)
    if (vm_) {
        (void)dtMs;
        vm_->execute("");
    }

    paintFrame();
}

std::string Tab::title() const {
    if (!document_) return url_;
    for (html::DOMNode* child = document_->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Element && child->tagName == "title") {
            std::string result;
            for (html::DOMNode* textChild = child->firstChild; textChild; textChild = textChild->nextSibling) {
                if (textChild->nodeType == html::NodeType::Text) {
                    result += textChild->textContent;
                }
            }
            return result;
        }
    }
    return url_;
}

void Tab::clear() {
    document_.reset();
    vm_.reset();
    url_.clear();
    rawHtml_.clear();
    loading_ = true;
}

js::VM* Tab::vm() const { return vm_.get(); }

// ── private helpers ──────────────────────────────────────────────────────

void Tab::parseHTML(const std::string& html) {
    html::HTMLParser parser;
    document_ = std::unique_ptr<html::Document>(parser.parse(html));
}

void Tab::cascadeStyles() {
    // Stub
    (void)document_;
}

void Tab::performLayout() {
    // Stub
    (void)document_;
}

void Tab::paintFrame() {
    if (!document_) {
        bodyText_ = "(no content loaded)";
        return;
    }
    html::DOMNode* bodyNode = nullptr;
    for (html::DOMNode* child = document_->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Element && child->tagName == "body") {
            bodyNode = child;
            break;
        }
    }
    bodyText_ = extractTextContent(bodyNode);
    paintUs_ = 0.0;
}

} // namespace browser

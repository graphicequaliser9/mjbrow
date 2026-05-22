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

void Tab::navigate(const std::string& url) {
    url_     = url;
    loading_ = true;

    util::Log(util::LogLevel::Info, "Tab::navigate → " + url + "\n");

    // Fetch raw HTML via the cross-platform net layer
    net::HttpClient client;
    auto response   = client.sendRequest(url, "GET");
    std::string body(response.body.begin(), response.body.end());
    rawHtml_ = body;

    if (rawHtml_.empty()) {
        util::Log(util::LogLevel::Warn, "Tab: empty response from " + url + "\n");
        loading_ = false;
        return;
    }

    parseHTML(rawHtml_);
    cascadeStyles();
    performLayout();
    loading_ = false;
}

void Tab::tick(double dtMs) {
    if (loading_ || !document_) return;

    // JS VM tick (requestAnimationFrame injection point in a full engine)
    if (vm_) {
        (void)dtMs;
        vm_->execute("");  // stub – real VM tick in later bead
        jsUs_ = 0.0;
    }

    // Repaint the active page every tick
    paintFrame();
    paintUs_ = 0.0;
}

std::string Tab::title() const {
    if (!document_) return url_;
    // Walk firstChild chain looking for <title>
    for (html::DOMNode* child = document_->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Element && child->tagName == "title") {
            return child->textContent;
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
    static std::unique_ptr<html::HTMLParser> parser;
    if (!parser) parser = std::make_unique<html::HTMLParser>();
    auto* doc = parser->parse(html);
    if (doc) doc->ownerDocument = static_cast<html::Document*>(doc);
    document_.reset(doc);
}

void Tab::cascadeStyles() {
    // Stub – real cascade in css/ module bead
    util::Log(util::LogLevel::Debug, "Tab: cascadeStyles stub\n");
    (void)document_;
}

void Tab::performLayout() {
    // Stub – real layout pass in layout/ bead
    util::Log(util::LogLevel::Debug, "Tab: performLayout stub\n");
    (void)document_;
}

void Tab::paintFrame() {
    // Stub – real paint pass calls Painter::fillRect / drawText
    if (document_) {
        paintUs_ = 0.0;
    }
}

} // namespace browser

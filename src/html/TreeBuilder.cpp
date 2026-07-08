#include "html/TreeBuilder.h"

#include <cctype>
#include <algorithm>

namespace html {

// ── helpers ────────────────────────────────────────────────────────────────────

std::string TreeBuilder::toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

const std::vector<std::string>& TreeBuilder::voidElements() {
    static const std::vector<std::string> v = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr"
    };
    return v;
}

bool TreeBuilder::isVoidElement(const std::string& tag) {
    std::string lower = toLower(tag);
    for (const auto& v : voidElements()) if (lower == v) return true;
    return false;
}

bool TreeBuilder::isFormattingElement(const std::string& tag) {
    static const std::vector<std::string> fmt = {
        "a", "abbr", "acronym", "b", "bdo", "big", "code", "del", "dfn",
        "em", "font", "i", "ins", "kbd", "mark", "nobr", "q", "s",
        "samp", "small", "span", "strike", "strong", "sub", "sup", "tt", "u", "var"
    };
    std::string lower = toLower(tag);
    for (const auto& f : fmt) if (lower == f) return true;
    return false;
}

bool TreeBuilder::isBlockLevelStart(const std::string& tag) {
    static const std::vector<std::string> blocks = {
        "address", "article", "aside", "blockquote", "center", "details",
        "dir", "div", "dl", "fieldset", "figcaption", "figure", "footer",
        "header", "hgroup", "main", "menu", "nav", "ol", "p", "section",
        "summary", "ul", "h1", "h2", "h3", "h4", "h5", "h6", "pre",
        "listing", "form", "li", "dd", "dt", "xmp", "table"
    };
    std::string lower = toLower(tag);
    for (const auto& b : blocks) if (lower == b) return true;
    return false;
}

// ── node manipulation ──────────────────────────────────────────────────────────

DOMNode* TreeBuilder::currentNode() const {
    return openElements_.empty() ? const_cast<Document*>(&document_) : openElements_.back();
}

DOMNode* TreeBuilder::ensureHtml() {
    DOMNode* html = nullptr;
    for (DOMNode* c = document_.firstChild; c; c = c->nextSibling) {
        if (c->nodeType == NodeType::Element && c->tagName == "html") { html = c; break; }
    }
    if (!html) {
        html = newElement("html", ns::HTML);
        appendChild(&document_, html);
    }
    bool onStack = false;
    for (DOMNode* e : openElements_) if (e == html) { onStack = true; break; }
    if (!onStack) openElements_.push_back(html);
    return html;
}

DOMNode* TreeBuilder::newElement(const std::string& tag, const std::string& namespaceURI) {
    auto* el = new DOMNode();
    el->nodeType = NodeType::Element;
    el->tagName = tag;
    el->namespaceURI = namespaceURI;
    el->ownerDocument = &document_;
    return el;
}

void TreeBuilder::appendChild(DOMNode* parent, DOMNode* child) {
    child->parent = parent;
    child->ownerDocument = &document_;
    child->prevSibling = parent->lastChild;
    child->nextSibling = nullptr;
    if (parent->lastChild) parent->lastChild->nextSibling = child;
    else parent->firstChild = child;
    parent->lastChild = child;
}

DOMNode* TreeBuilder::insertElement(const std::string& tag, const Token& tok, const std::string& ns) {
    DOMNode* parent = currentNode();
    DOMNode* el = newElement(tag, ns);
    for (const auto& a : tok.attributes) el->attributes[a.first] = a.second;
    appendChild(parent, el);
    if (!isVoidElement(tag)) openElements_.push_back(el);
    return el;
}

void TreeBuilder::insertText(const std::string& text) {
    if (text.empty()) return;
    DOMNode* parent = currentNode();
    if (parent->lastChild && parent->lastChild->nodeType == NodeType::Text) {
        parent->lastChild->textContent += text;
        return;
    }
    auto* t = new DOMNode();
    t->nodeType = NodeType::Text;
    t->textContent = text;
    t->ownerDocument = &document_;
    appendChild(parent, t);
}

void TreeBuilder::insertComment(const std::string& text) {
    DOMNode* parent = currentNode();
    auto* c = new DOMNode();
    c->nodeType = NodeType::Comment;
    c->textContent = text;
    c->ownerDocument = &document_;
    appendChild(parent, c);
}

void TreeBuilder::pop() {
    if (!openElements_.empty()) openElements_.pop_back();
}

DOMNode* TreeBuilder::popUntil(const std::string& tag) {
    for (auto it = openElements_.rbegin(); it != openElements_.rend(); ++it) {
        if ((*it)->tagName == tag) {
            DOMNode* found = *it;
            openElements_.erase(std::next(it).base(), openElements_.end());
            return found;
        }
    }
    return nullptr;
}

void TreeBuilder::popUntilNode(DOMNode* node) {
    while (!openElements_.empty() && openElements_.back() != node) {
        openElements_.pop_back();
    }
    pop();
}

bool TreeBuilder::inScope(const std::string& tag) const {
    for (auto it = openElements_.rbegin(); it != openElements_.rend(); ++it) {
        if ((*it)->tagName == tag) return true;
    }
    return false;
}

std::string TreeBuilder::namespaceFor(const std::string& tag) const {
    std::string lower = toLower(tag);
    if (lower == "svg")  return ns::SVG;
    if (lower == "math") return ns::MathML;
    DOMNode* cur = currentNode();
    if (cur->nodeType == NodeType::Element && !cur->namespaceURI.empty()) {
        return cur->namespaceURI;
    }
    return ns::HTML;
}

// ── foster parenting ──────────────────────────────────────────────────────────

DOMNode* TreeBuilder::fosterParentTable() const {
    for (auto it = openElements_.rbegin(); it != openElements_.rend(); ++it) {
        if ((*it)->tagName == "table") return *it;
    }
    return nullptr;
}

void TreeBuilder::fosterParentInsertComment(const std::string& text) {
    DOMNode* table = fosterParentTable();
    if (!table) { insertComment(text); return; }

    if (!table->parent) { insertComment(text); return; }

    auto* c = new DOMNode();
    c->nodeType = NodeType::Comment;
    c->textContent = text;
    c->ownerDocument = &document_;

    DOMNode* parent = table->parent;
    c->nextSibling = table;
    c->prevSibling = table->prevSibling;
    if (table->prevSibling) {
        table->prevSibling->nextSibling = c;
    } else {
        parent->firstChild = c;
    }
    table->prevSibling = c;
}

void TreeBuilder::fosterParentInsertText(const std::string& text) {
    DOMNode* table = fosterParentTable();
    if (!table) { insertText(text); return; }

    if (table->parent && table->prevSibling &&
        table->prevSibling->nodeType == NodeType::Text) {
        table->prevSibling->textContent += text;
        return;
    }

    auto* t = new DOMNode();
    t->nodeType = NodeType::Text;
    t->textContent = text;
    t->ownerDocument = &document_;

    if (!table->parent) { insertText(text); return; }

    DOMNode* parent = table->parent;
    t->nextSibling = table;
    t->prevSibling = table->prevSibling;
    if (table->prevSibling) {
        table->prevSibling->nextSibling = t;
    } else {
        parent->firstChild = t;
    }
    table->prevSibling = t;
}

void TreeBuilder::fosterParentInsertElement(const std::string& tag, const Token& tok, const std::string& ns) {
    DOMNode* table = fosterParentTable();
    if (!table) { insertElement(tag, tok, ns); return; }

    if (!table->parent) { insertElement(tag, tok, ns); return; }

    DOMNode* el = newElement(tag, ns);
    for (const auto& a : tok.attributes) el->attributes[a.first] = a.second;

    DOMNode* parent = table->parent;
    el->nextSibling = table;
    el->prevSibling = table->prevSibling;
    if (table->prevSibling) {
        table->prevSibling->nextSibling = el;
    } else {
        parent->firstChild = el;
    }
    table->prevSibling = el;
    el->parent = parent;
    el->ownerDocument = &document_;

    if (!isVoidElement(tag)) {
        openElements_.push_back(el);
    }
}

// ── adoption agency ────────────────────────────────────────────────────────────

void TreeBuilder::adoptionAgency(const std::string& tag) {
    std::string lower = toLower(tag);
    auto fmtIt = std::find_if(activeFormatting_.rbegin(), activeFormatting_.rend(),
        [&](DOMNode* n){ return n->tagName == lower; });
    if (fmtIt == activeFormatting_.rend()) return;

    DOMNode* fmtEl = *fmtIt;
    auto openIt = std::find(openElements_.rbegin(), openElements_.rend(), fmtEl);
    if (openIt == openElements_.rend()) {
        activeFormatting_.erase(std::next(fmtIt).base());
        return;
    }

    popUntilNode(fmtEl);
    activeFormatting_.erase(std::next(fmtIt).base());

    DOMNode* fresh = newElement(fmtEl->tagName, fmtEl->namespaceURI);
    fresh->attributes = fmtEl->attributes;
    fresh->ownerDocument = &document_;
    appendChild(currentNode(), fresh);
    openElements_.push_back(fresh);
}

// ── token dispatch ────────────────────────────────────────────────────────────

void TreeBuilder::processToken(const Token& tok) {
    switch (tok.type) {
        case TokenType::DOCTYPE:  processDoctype(tok); break;
        case TokenType::Comment:  processComment(tok); break;
        case TokenType::StartTag: processStartTag(tok); break;
        case TokenType::EndTag:   processEndTag(tok);  break;
        case TokenType::Character: processCharacter(tok); break;
        case TokenType::EOF_TOKEN: processEof(tok); break;
    }
}

void TreeBuilder::processDoctype(const Token& tok) {
    document_.doctype = tok.data;
}

void TreeBuilder::processComment(const Token& tok) {
    if (mode_ == Mode::InTable) {
        fosterParentInsertComment(tok.data);
        return;
    }
    insertComment(tok.data);
}

void TreeBuilder::processCharacter(const Token& tok) {
    if (mode_ == Mode::Initial) return;
    if (mode_ == Mode::BeforeHtml) return;
    if (mode_ == Mode::Text) { pendingText_ += tok.data; return; }

    if (mode_ == Mode::InTable) {
        fosterParentInsertText(tok.data);
        return;
    }
    insertText(tok.data);
}

void TreeBuilder::processEof(const Token& /*tok*/) {
    switch (mode_) {
        case Mode::Text:
            insertText(pendingText_);
            pop();
            mode_ = textReturnMode_;
            break;
        case Mode::InBody:
        case Mode::AfterBody:
            mode_ = Mode::AfterAfterBody;
            break;
        case Mode::InTable:
        case Mode::InTableBody:
        case Mode::InRow:
        case Mode::InCell:
            while (!openElements_.empty()) pop();
            mode_ = Mode::AfterBody;
            break;
        default:
            break;
    }
}

// ── start tags ────────────────────────────────────────────────────────────────

void TreeBuilder::processStartTag(const Token& tok) {
    std::string tag = toLower(tok.data);

    switch (mode_) {
        case Mode::Initial:
            if (tag == "html") { startHtml(tok); }
            else { startHtml(Token{TokenType::StartTag, "html", {}}); reprocess(tok); }
            break;
        case Mode::BeforeHtml:
            if (tag == "html") { startHtml(tok); }
            else if (tag == "head") { enterHead(tok); }
            else { ensureHtml(); enterHead(Token{TokenType::StartTag, "head", {}}); reprocess(tok); }
            break;
        case Mode::BeforeHead:
            if (tag == "head") { enterHead(tok); }
            else if (tag == "html") { startHtml(tok); }
            else {
                ensureHtml();
                auto* head = newElement("head", ns::HTML);
                appendChild(currentNode(), head);
                openElements_.push_back(head);
                mode_ = Mode::InHead;
                reprocess(tok);
            }
            break;
        case Mode::InHead:
            if (tag == "head") return;
            else if (tag == "title" || tag == "style" || tag == "script" ||
                     tag == "noscript" || tag == "template" || tag == "textarea") {
                startRawText(tok, tag);
            } else if (tag == "/head" || tag == "body" || tag == "html" ||
                       tag == "br" || tag == "meta" || tag == "link" ||
                       tag == "base") {
                closeHead();
                reprocess(tok);
            } else {
                closeHead();
                reprocess(tok);
            }
            break;
        case Mode::AfterHead:
            if (tag == "body") { startBody(tok); }
            else if (tag == "html") { startHtml(tok); }
            else { ensureHtml(); startBody(Token{TokenType::StartTag, "body", {}}); reprocess(tok); }
            break;
        case Mode::InBody:
            processInBodyStart(tok, tag);
            break;
        case Mode::InTable:
            processInTableStart(tok, tag);
            break;
        case Mode::InTableBody:
            processInTableBodyStart(tok, tag);
            break;
        case Mode::InRow:
            processInRowStart(tok, tag);
            break;
        case Mode::InCell:
            processInBodyStart(tok, tag);
            break;
        case Mode::AfterBody:
            if (tag == "html") { startHtml(tok); }
            else { mode_ = Mode::InBody; reprocess(tok); }
            break;
        case Mode::AfterAfterBody:
            mode_ = Mode::InBody; reprocess(tok);
            break;
        case Mode::Text:
            break;
    }
}

void TreeBuilder::reprocess(const Token& tok) { processToken(tok); }

void TreeBuilder::startHtml(const Token& tok) {
    ensureHtml();
    DOMNode* h = document_.firstChild;
    for (const auto& a : tok.attributes) h->attributes[a.first] = a.second;
    mode_ = Mode::BeforeHead;
}

void TreeBuilder::enterHead(const Token& tok) {
    DOMNode* html = ensureHtml();
    auto* head = newElement("head", ns::HTML);
    appendChild(html, head);
    openElements_.push_back(head);
    mode_ = Mode::InHead;
    for (const auto& a : tok.attributes) head->attributes[a.first] = a.second;
}

void TreeBuilder::closeHead() {
    if (!openElements_.empty() && openElements_.back()->tagName == "head") pop();
    mode_ = Mode::AfterHead;
}

void TreeBuilder::startBody(const Token& tok) {
    DOMNode* html = ensureHtml();
    auto* body = newElement("body", ns::HTML);
    for (const auto& a : tok.attributes) body->attributes[a.first] = a.second;
    appendChild(html, body);
    openElements_.push_back(body);
    mode_ = Mode::InBody;
}

void TreeBuilder::startRawText(const Token& tok, const std::string& /*tag*/) {
    std::string nsUri = namespaceFor(tok.data);
    DOMNode* el = insertElement(toLower(tok.data), tok, nsUri);
    (void)el;
    textReturnMode_ = mode_;
    pendingText_.clear();
    mode_ = Mode::Text;
}

// ── body insertion mode ───────────────────────────────────────────────────────

void TreeBuilder::processInBodyStart(const Token& tok, const std::string& tag) {
    // Auto-close an open <p> when a block-level start tag arrives.
    if (inScope("p") && (tag == "p" || isBlockLevelStart(tag))) {
        popUntil("p");
    }

    if (isFormattingElement(tag)) {
        adoptionAgency(tag);
        auto* fmt = insertElement(tag, tok, namespaceFor(tag));
        activeFormatting_.push_back(fmt);
        return;
    }

    if (tag == "table") {
        DOMNode* el = insertElement(tag, tok, ns::HTML);
        (void)el;
        mode_ = Mode::InTable;
        return;
    }
    if (tag == "script" || tag == "style" || tag == "title" || tag == "textarea" ||
        tag == "noscript") {
        startRawText(tok, tag);
        return;
    }
    insertElement(tag, tok, namespaceFor(tag));
}

// ── table insertion modes ─────────────────────────────────────────────────────

void TreeBuilder::processInTableStart(const Token& tok, const std::string& tag) {
    if (tag == "tbody" || tag == "thead" || tag == "tfoot") {
        insertElement(tag, tok, ns::HTML);
        mode_ = Mode::InTableBody;
    } else if (tag == "tr") {
        auto* tbody = insertElement("tbody", Token{TokenType::StartTag, "tbody", {}}, ns::HTML);
        (void)tbody;
        insertElement("tr", tok, ns::HTML);
        mode_ = Mode::InRow;
    } else if (tag == "td" || tag == "th") {
        insertElement("tbody", Token{TokenType::StartTag, "tbody", {}}, ns::HTML);
        insertElement("tr", Token{TokenType::StartTag, "tr", {}}, ns::HTML);
        insertElement(tag, tok, ns::HTML);
        mode_ = Mode::InCell;
    } else if (tag == "caption") {
        insertElement(tag, tok, ns::HTML);
        mode_ = Mode::InRow;
    } else if (tag == "colgroup" || tag == "col") {
        insertElement(tag, tok, ns::HTML);
    } else {
        fosterParentInsertElement(tag, tok, namespaceFor(tag));
    }
}

void TreeBuilder::processInTableBodyStart(const Token& tok, const std::string& tag) {
    if (tag == "tr") {
        insertElement(tag, tok, ns::HTML);
        mode_ = Mode::InRow;
    } else if (tag == "td" || tag == "th") {
        insertElement("tr", Token{TokenType::StartTag, "tr", {}}, ns::HTML);
        insertElement(tag, tok, ns::HTML);
        mode_ = Mode::InCell;
    } else if (tag == "tbody" || tag == "thead" || tag == "tfoot") {
        insertElement(tag, tok, ns::HTML);
    } else {
        fosterParentInsertElement(tag, tok, namespaceFor(tag));
    }
}

void TreeBuilder::processInRowStart(const Token& tok, const std::string& tag) {
    if (tag == "td" || tag == "th") {
        insertElement(tag, tok, ns::HTML);
        mode_ = Mode::InCell;
    } else {
        fosterParentInsertElement(tag, tok, namespaceFor(tag));
    }
}

// ── end tags ──────────────────────────────────────────────────────────────────

void TreeBuilder::processEndTag(const Token& tok) {
    std::string tag = toLower(tok.data);

    if (mode_ == Mode::Text) {
        DOMNode* cur = currentNode();
        if (cur->nodeType == NodeType::Element && cur->tagName == tag) {
            insertText(pendingText_);
            pop();
            mode_ = textReturnMode_;
        }
        return;
    }

    switch (mode_) {
        case Mode::InHead:
            if (tag == "head") closeHead();
            break;
        case Mode::InBody:
            processInBodyEnd(tag);
            break;
        case Mode::InTable:
            if (tag == "table") { if (popUntil("table")) mode_ = Mode::AfterBody; }
            else if (tag == "tbody" || tag == "thead" || tag == "tfoot") {
                popUntil(tag);
            }
            break;
        case Mode::InTableBody:
            if (tag == "tbody" || tag == "thead" || tag == "tfoot") {
                popUntil(tag); mode_ = Mode::InTable;
            } else if (tag == "table") {
                while (openElements_.size() > 1 &&
                       openElements_.back()->tagName != "table") pop();
                if (openElements_.back()->tagName == "table") pop();
                mode_ = Mode::InTable;
            }
            break;
        case Mode::InRow:
            if (tag == "tr") {
                popUntil("tr"); mode_ = Mode::InTableBody;
            } else if (tag == "tbody" || tag == "thead" || tag == "tfoot" || tag == "table") {
                while (openElements_.size() > 1 &&
                       openElements_.back()->tagName != "table") pop();
                if (openElements_.back()->tagName == "table") pop();
                mode_ = Mode::InTable;
            }
            break;
        case Mode::InCell:
            if (tag == "td" || tag == "th") {
                popUntil(tag); mode_ = Mode::InRow;
            } else if (tag == "tr") {
                while (openElements_.size() > 1 &&
                       openElements_.back()->tagName != "tr") pop();
                if (openElements_.back()->tagName == "tr") pop();
                mode_ = Mode::InTableBody;
            } else if (tag == "tbody" || tag == "thead" || tag == "tfoot" || tag == "table") {
                while (openElements_.size() > 1 &&
                       openElements_.back()->tagName != "table") pop();
                if (openElements_.back()->tagName == "table") pop();
                mode_ = Mode::InTable;
            }
            break;
        case Mode::AfterBody:
            if (tag == "html") mode_ = Mode::AfterAfterBody;
            break;
        case Mode::AfterAfterBody:
            if (tag == "html") { /* stay */ }
            break;
        default:
            break;
    }
}

void TreeBuilder::processInBodyEnd(const std::string& tag) {
    if (isVoidElement(tag)) return;

    if (isFormattingElement(tag)) {
        for (auto it = activeFormatting_.begin(); it != activeFormatting_.end(); ++it) {
            if ((*it)->tagName == tag) { activeFormatting_.erase(it); break; }
        }
    }

    if (inScope(tag)) {
        popUntil(tag);
    }
}

// ── builder entry point ───────────────────────────────────────────────────────

TreeBuilder::TreeBuilder(Document& doc) : document_(doc) {}

void TreeBuilder::build(const std::vector<Token>& tokens) {
    for (const Token& tok : tokens) {
        processToken(tok);
        if (tok.type == TokenType::EOF_TOKEN) break;
    }
}

} // namespace html

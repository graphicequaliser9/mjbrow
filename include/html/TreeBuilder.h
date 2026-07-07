/**
 * @file html/TreeBuilder.h
 * @brief HTML5 tree builder (parser).
 * @details Consumes a token stream (from html::Tokenizer) and constructs a
 *          real DOM tree rooted at an html::Document.  Implements insertion
 *          modes, an implicit <tbody> rule for tables, a simplified adoption
 *          agency algorithm for formatting elements, and namespace splitting
 *          for SVG / MathML subtrees.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef HTML_TREEBUILDER_H
#define HTML_TREEBUILDER_H

#include <string>
#include <vector>
#include <cctype>
#include <algorithm>

#include "html/Tokenizer.h"
#include "html/DOMNode.h"

namespace html {

/**
 * @brief Well-known namespaces used by the tree builder.
 */
namespace ns {
constexpr const char* HTML = "http://www.w3.org/1999/xhtml";
constexpr const char* SVG  = "http://www.w3.org/2000/svg";
constexpr const char* MathML = "http://www.w3.org/1998/Math/MathML";
}

/**
 * @class TreeBuilder
 * @brief Builds a DOM tree from a token stream.
 */
class TreeBuilder {
public:
    /**
     * @brief Constructs a tree builder that appends children to @p doc.
     * @param doc The owner document to populate.
     */
    explicit TreeBuilder(Document& doc)
        : document_(doc) {}

    /**
     * @brief Runs the parser over the token stream, populating the document.
     * @param tokens A token stream ending in EOF_TOKEN.
     */
    void build(const std::vector<Token>& tokens) {
        for (const Token& tok : tokens) {
            processToken(tok);
            if (tok.type == TokenType::EOF_TOKEN) break;
        }
    }

private:
    // ── insertion modes ──────────────────────────────────────────────────────
    enum class Mode {
        Initial,
        BeforeHtml,
        BeforeHead,
        InHead,
        AfterHead,
        InBody,
        InTable,
        InTableBody,
        InRow,
        InCell,
        AfterBody,
        AfterAfterBody,
        Text,          ///< inside RCDATA / RAWTEXT element (title, script, style, ...)
    };

    Document& document_;
    std::vector<DOMNode*> openElements_;            ///< stack of open element nodes
    std::vector<DOMNode*> activeFormatting_;        ///< formatting elements (simplified)
    Mode mode_{Mode::Initial};
    Mode textReturnMode_{Mode::InBody};            ///< mode to restore after Text mode
    std::string pendingText_;                       ///< buffered text while in Text mode

    static const std::vector<std::string>& voidElements();
    static bool isVoidElement(const std::string& tag);
    static bool isFormattingElement(const std::string& tag);
    static std::string toLower(std::string s);

    // ── node manipulation ─────────────────────────────────────────────────────
    DOMNode* currentNode() const {
        return openElements_.empty() ? const_cast<Document*>(&document_) : openElements_.back();
    }

    DOMNode* ensureHtml() {
        // Document's first element child is <html>.
        DOMNode* html = nullptr;
        for (DOMNode* c = document_.firstChild; c; c = c->nextSibling) {
            if (c->nodeType == NodeType::Element && c->tagName == "html") { html = c; break; }
        }
        if (!html) {
            html = newElement("html", ns::HTML);
            appendChild(&document_, html);
        }
        // Keep <html> as the bottom of the open-element stack so that head/body
        // and other content attach to it rather than to the document root.
        bool onStack = false;
        for (DOMNode* e : openElements_) if (e == html) { onStack = true; break; }
        if (!onStack) openElements_.push_back(html);
        return html;
    }

    DOMNode* newElement(const std::string& tag, const std::string& namespaceURI) {
        auto* el = new DOMNode();
        el->nodeType = NodeType::Element;
        el->tagName = tag;
        el->namespaceURI = namespaceURI;
        el->ownerDocument = &document_;
        return el;
    }

    void appendChild(DOMNode* parent, DOMNode* child) {
        child->parent = parent;
        child->ownerDocument = &document_;
        child->prevSibling = parent->lastChild;
        child->nextSibling = nullptr;
        if (parent->lastChild) parent->lastChild->nextSibling = child;
        else parent->firstChild = child;
        parent->lastChild = child;
    }

    DOMNode* insertElement(const std::string& tag, const Token& tok, const std::string& ns) {
        DOMNode* parent = currentNode();
        DOMNode* el = newElement(tag, ns);
        for (const auto& a : tok.attributes) el->attributes[a.first] = a.second;
        appendChild(parent, el);
        if (!isVoidElement(tag)) openElements_.push_back(el);
        return el;
    }

    void insertText(const std::string& text) {
        if (text.empty()) return;
        DOMNode* parent = currentNode();
        // Coalesce adjacent text nodes.
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

    void insertComment(const std::string& text) {
        DOMNode* parent = currentNode();
        auto* c = new DOMNode();
        c->nodeType = NodeType::Comment;
        c->textContent = text;
        c->ownerDocument = &document_;
        appendChild(parent, c);
    }

    void pop() {
        if (!openElements_.empty()) openElements_.pop_back();
    }

    /// @brief Pops the open-element stack until (and including) the nearest
    ///        element with the given tag, then returns it (or nullptr).
    DOMNode* popUntil(const std::string& tag) {
        for (auto it = openElements_.rbegin(); it != openElements_.rend(); ++it) {
            if ((*it)->tagName == tag) {
                DOMNode* found = *it;
                openElements_.erase(std::next(it).base(), openElements_.end());
                return found;
            }
        }
        return nullptr;
    }

    /// @brief Pops the open-element stack until the top equals @p node (inclusive).
    void popUntilNode(DOMNode* node) {
        while (!openElements_.empty() && openElements_.back() != node) {
            openElements_.pop_back();
        }
        pop();
    }

    bool inScope(const std::string& tag) const {
        for (auto it = openElements_.rbegin(); it != openElements_.rend(); ++it) {
            if ((*it)->tagName == tag) return true;
        }
        return false;
    }

    // ── namespace resolution ──────────────────────────────────────────────────
    std::string namespaceFor(const std::string& tag) const {
        std::string lower = toLower(tag);
        if (lower == "svg")  return ns::SVG;
        if (lower == "math") return ns::MathML;
        DOMNode* cur = currentNode();
        if (cur->nodeType == NodeType::Element && !cur->namespaceURI.empty()) {
            return cur->namespaceURI;
        }
        return ns::HTML;
    }

    // ── adoption agency (simplified) ──────────────────────────────────────────
    void adoptionAgency(const std::string& tag) {
        // Simplified version: if the formatting element is already on the stack
        // of open elements, pop it and re-adopt its children into a fresh element
        // of the same tag, then push that element back onto both stacks.
        std::string lower = toLower(tag);
        auto fmtIt = std::find_if(activeFormatting_.rbegin(), activeFormatting_.rend(),
            [&](DOMNode* n){ return n->tagName == lower; });
        if (fmtIt == activeFormatting_.rend()) return;

        DOMNode* fmtEl = *fmtIt;
        auto openIt = std::find(openElements_.rbegin(), openElements_.rend(), fmtEl);
        if (openIt == openElements_.rend()) {
            // Not currently open; drop from formatting list and bail.
            activeFormatting_.erase(std::next(fmtIt).base());
            return;
        }

        // Close the element where it currently sits, then re-open a fresh copy
        // adopting the remaining children (simplified: just re-insert it).
        popUntilNode(fmtEl);
        // Remove from formatting list.
        activeFormatting_.erase(std::next(fmtIt).base());
        // Re-open so subsequent content is adopted inside it.
        DOMNode* fresh = newElement(fmtEl->tagName, fmtEl->namespaceURI);
        fresh->attributes = fmtEl->attributes;
        fresh->ownerDocument = &document_;
        appendChild(currentNode(), fresh);
        openElements_.push_back(fresh);
    }

    // ── token dispatch ────────────────────────────────────────────────────────
    void processToken(const Token& tok) {
        switch (tok.type) {
            case TokenType::DOCTYPE:  processDoctype(tok); break;
            case TokenType::Comment:  processComment(tok); break;
            case TokenType::StartTag: processStartTag(tok); break;
            case TokenType::EndTag:   processEndTag(tok);  break;
            case TokenType::Character: processCharacter(tok); break;
            case TokenType::EOF_TOKEN: processEof(tok); break;
        }
    }

    void processDoctype(const Token& /*tok*/) {
        // DOCTYPE is ignored; we always build a standards-mode document.
    }

    void processComment(const Token& tok) {
        if (mode_ == Mode::Initial || mode_ == Mode::BeforeHtml) {
            insertComment(tok.data);   // attach to Document
        } else {
            insertComment(tok.data);
        }
    }

    void processCharacter(const Token& tok) {
        if (mode_ == Mode::Initial) return;            // leading whitespace before <html>
        if (mode_ == Mode::BeforeHtml) return;
        if (mode_ == Mode::Text) { pendingText_ += tok.data; return; }
        insertText(tok.data);
    }

    void processEof(const Token& /*tok*/) {
        switch (mode_) {
            case Mode::Text:
                // Close the RCDATA/RAWTEXT element.
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
                // Close any open table-structure elements on EOF.
                while (!openElements_.empty()) pop();
                mode_ = Mode::AfterBody;
                break;
            default:
                break;
        }
    }

    // ── start tags ────────────────────────────────────────────────────────────
    void processStartTag(const Token& tok) {
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
                else { // any other tag → implied empty head, then body-level processing
                    ensureHtml();
                    auto* head = newElement("head", ns::HTML);
                    appendChild(currentNode(), head);
                    openElements_.push_back(head);
                    mode_ = Mode::InHead;
                    reprocess(tok);
                }
                break;
            case Mode::InHead:
                if (tag == "head") return; // stray
                else if (tag == "title" || tag == "style" || tag == "script" ||
                         tag == "noscript" || tag == "template" || tag == "textarea") {
                    startRawText(tok, tag);
                } else if (tag == "/head" || tag == "body" || tag == "html" ||
                           tag == "br" || tag == "meta" || tag == "link" ||
                           tag == "base") {
                    // close head and move on
                    closeHead();
                    reprocess(tok);
                } else {
                    // Non-head content (e.g. stray text element) → leave head.
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
                processInBodyStart(tok, tag);   // cell contents behave like body
                break;
            case Mode::AfterBody:
                if (tag == "html") { startHtml(tok); }
                else { mode_ = Mode::InBody; reprocess(tok); }
                break;
            case Mode::AfterAfterBody:
                mode_ = Mode::InBody; reprocess(tok);
                break;
            case Mode::Text:
                // Text mode only exits on end tag; ignore start tags.
                break;
        }
    }

    // Re-process a token in the (possibly new) current mode.
    void reprocess(const Token& tok) { processToken(tok); }

    void startHtml(const Token& tok) {
        ensureHtml();
        // ensureHtml may have created a fresh element; re-find to set attrs.
        DOMNode* h = document_.firstChild;
        for (const auto& a : tok.attributes) h->attributes[a.first] = a.second;
        mode_ = Mode::BeforeHead;
    }

    void enterHead(const Token& tok) {
        DOMNode* html = ensureHtml();
        auto* head = newElement("head", ns::HTML);
        appendChild(html, head);
        openElements_.push_back(head);
        mode_ = Mode::InHead;
        // process head's own attributes if present
        for (const auto& a : tok.attributes) head->attributes[a.first] = a.second;
    }

    void closeHead() {
        if (!openElements_.empty() && openElements_.back()->tagName == "head") pop();
        mode_ = Mode::AfterHead;
    }

    void startBody(const Token& tok) {
        DOMNode* html = ensureHtml();
        auto* body = newElement("body", ns::HTML);
        for (const auto& a : tok.attributes) body->attributes[a.first] = a.second;
        appendChild(html, body);
        openElements_.push_back(body);
        mode_ = Mode::InBody;
    }

    void startRawText(const Token& tok, const std::string& /*tag*/) {
        std::string nsUri = namespaceFor(tok.data);
        DOMNode* el = insertElement(toLower(tok.data), tok, nsUri);
        (void)el;
        textReturnMode_ = mode_;
        pendingText_.clear();
        mode_ = Mode::Text;
    }

    // ── main "in body" start-tag handling ─────────────────────────────────────
    void processInBodyStart(const Token& tok, const std::string& tag) {
        if (isFormattingElement(tag)) {
            adoptionAgency(tag);
            auto* fmt = insertElement(tag, tok, namespaceFor(tag));
            activeFormatting_.push_back(fmt);
            return;
        }

        // Table entry point.
        if (tag == "table") {
            DOMNode* el = insertElement(tag, tok, ns::HTML);
            (void)el;
            mode_ = Mode::InTable;
            return;
        }
        // Raw-text / RCDATA elements inside body.
        if (tag == "script" || tag == "style" || tag == "title" || tag == "textarea" ||
            tag == "noscript") {
            startRawText(tok, tag);
            return;
        }
        // Ordinary element.
        insertElement(tag, tok, namespaceFor(tag));
    }

    // ── table insertion modes ─────────────────────────────────────────────────
    void processInTableStart(const Token& tok, const std::string& tag) {
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
            mode_ = Mode::InRow;   // simplified: treat caption like a row-level container
        } else if (tag == "colgroup" || tag == "col") {
            insertElement(tag, tok, ns::HTML);
        } else {
            // Anything else: fall back to body-style insertion under current node.
            insertElement(tag, tok, namespaceFor(tag));
        }
    }

    void processInTableBodyStart(const Token& tok, const std::string& tag) {
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
            insertElement(tag, tok, namespaceFor(tag));
        }
    }

    void processInRowStart(const Token& tok, const std::string& tag) {
        if (tag == "td" || tag == "th") {
            insertElement(tag, tok, ns::HTML);
            mode_ = Mode::InCell;
        } else {
            // Implicit <tr> handling for stray content.
            insertElement(tag, tok, namespaceFor(tag));
        }
    }

    // ── end tags ──────────────────────────────────────────────────────────────
    void processEndTag(const Token& tok) {
        std::string tag = toLower(tok.data);

        if (mode_ == Mode::Text) {
            // Only the matching end tag closes RCDATA/RAWTEXT.
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

    void processInBodyEnd(const std::string& tag) {
        if (isVoidElement(tag)) return;   // void elements cannot be closed

        // Closing a formatting element: pop it from the formatting list and
        // close the corresponding open element.
        if (isFormattingElement(tag)) {
            for (auto it = activeFormatting_.begin(); it != activeFormatting_.end(); ++it) {
                if ((*it)->tagName == tag) { activeFormatting_.erase(it); break; }
            }
        }

        if (inScope(tag)) {
            popUntil(tag);
        }
        // Otherwise: ignore stray end tag.
    }
};

// ── static helpers ────────────────────────────────────────────────────────────

inline const std::vector<std::string>& TreeBuilder::voidElements() {
    static const std::vector<std::string> v = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr"
    };
    return v;
}

inline bool TreeBuilder::isVoidElement(const std::string& tag) {
    std::string lower = toLower(tag);
    for (const auto& v : voidElements()) if (lower == v) return true;
    return false;
}

inline bool TreeBuilder::isFormattingElement(const std::string& tag) {
    static const std::vector<std::string> fmt = {
        "a", "b", "big", "code", "em", "font", "i", "nobr",
        "s", "small", "strike", "strong", "tt", "u"
    };
    std::string lower = toLower(tag);
    for (const auto& f : fmt) if (lower == f) return true;
    return false;
}

inline std::string TreeBuilder::toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace html

#endif // HTML_TREEBUILDER_H

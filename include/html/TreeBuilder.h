/**
 * @file html/TreeBuilder.h
 * @brief HTML5 tree builder (parser) declarations.
 * @details Consumes a token stream (from html::Tokenizer) and constructs a
 *          real DOM tree rooted at an html::Document.  Implementations live in
 *          src/html/TreeBuilder.cpp.
 */

#ifndef HTML_TREEBUILDER_H
#define HTML_TREEBUILDER_H

#include <string>
#include <vector>

#include "html/Tokenizer.h"
#include "html/DOMNode.h"

namespace html {

namespace ns {
constexpr const char* HTML    = "http://www.w3.org/1999/xhtml";
constexpr const char* SVG     = "http://www.w3.org/2000/svg";
constexpr const char* MathML  = "http://www.w3.org/1998/Math/MathML";
}

class TreeBuilder {
public:
    explicit TreeBuilder(Document& doc);

    void build(const std::vector<Token>& tokens);

    static const std::vector<std::string>& voidElements();
    static bool isVoidElement(const std::string& tag);
    static bool isFormattingElement(const std::string& tag);
    static bool isBlockLevelStart(const std::string& tag);
    static std::string toLower(std::string s);

private:
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
        Text,
    };

    Document& document_;
    std::vector<DOMNode*> openElements_;
    std::vector<DOMNode*> activeFormatting_;
    Mode mode_{Mode::Initial};
    Mode textReturnMode_{Mode::InBody};
    std::string pendingText_;

    DOMNode* currentNode() const;
    DOMNode* ensureHtml();
    DOMNode* newElement(const std::string& tag, const std::string& namespaceURI);
    void appendChild(DOMNode* parent, DOMNode* child);
    DOMNode* insertElement(const std::string& tag, const Token& tok, const std::string& ns);
    void insertText(const std::string& text);
    void insertComment(const std::string& text);
    void pop();
    DOMNode* popUntil(const std::string& tag);
    void popUntilNode(DOMNode* node);
    bool inScope(const std::string& tag) const;
    std::string namespaceFor(const std::string& tag) const;
    void adoptionAgency(const std::string& tag);

    void fosterParentInsertText(const std::string& text);
    void fosterParentInsertComment(const std::string& text);
    void fosterParentInsertElement(const std::string& tag, const Token& tok, const std::string& ns);

    DOMNode* fosterParentTable() const;

    void processToken(const Token& tok);
    void processDoctype(const Token& tok);
    void processComment(const Token& tok);
    void processCharacter(const Token& tok);
    void processEof(const Token& tok);
    void processStartTag(const Token& tok);
    void processEndTag(const Token& tok);
    void reprocess(const Token& tok);

    void startHtml(const Token& tok);
    void enterHead(const Token& tok);
    void closeHead();
    void startBody(const Token& tok);
    void startRawText(const Token& tok, const std::string& tag);

    void processInBodyStart(const Token& tok, const std::string& tag);
    void processInTableStart(const Token& tok, const std::string& tag);
    void processInTableBodyStart(const Token& tok, const std::string& tag);
    void processInRowStart(const Token& tok, const std::string& tag);
    void processInBodyEnd(const std::string& tag);
};

} // namespace html

#endif // HTML_TREEBUILDER_H

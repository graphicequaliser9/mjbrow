/**
 * @file HTMLParser.cpp
 * @brief HTML5 parser entry point: orchestrates tokeniser + tree builder.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/HTMLParser.h"
#include "html/Tokenizer.h"
#include "html/TreeBuilder.h"

#include <string>
#include <vector>

namespace html {

Document* HTMLParser::parse(const std::string& html) {
    auto* doc = new Document();
    Tokenizer tokenizer(html);
    std::vector<Token> tokens = tokenizer.tokenize();
    TreeBuilder builder(*doc);
    builder.build(tokens);
    return doc;
}

void HTMLParser::parseInto(Document& doc, const std::string& html) {
    while (doc.firstChild) {
        DOMNode* next = doc.firstChild->nextSibling;
        delete doc.firstChild;
        doc.firstChild = next;
    }
    doc.lastChild = nullptr;

    Tokenizer tokenizer(html);
    std::vector<Token> tokens = tokenizer.tokenize();
    TreeBuilder builder(doc);
    builder.build(tokens);
}

HTMLParser::HTMLParser() = default;
HTMLParser::~HTMLParser() = default;

} // namespace html

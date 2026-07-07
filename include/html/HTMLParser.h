/**
 * @file HTMLParser.h
 * @brief HTML5 parser entry point.
 * @details Orchestrates the tokeniser (html/Tokenizer.h) and tree builder
 *          (html/TreeBuilder.h) to produce a real DOM tree rooted at a
 *          html::Document.  This header is a thin wrapper; the heavy lifting
 *          lives in Tokenizer.h and TreeBuilder.h.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef HTML_HTMLPARSER_H
#define HTML_HTMLPARSER_H

#include <string>
#include <vector>

#include "html/Tokenizer.h"
#include "html/DOMNode.h"

namespace html {

class HTMLParser {
public:
    HTMLParser();
    ~HTMLParser();

    /**
     * @brief Parses HTML input and returns a freshly-allocated Document.
     * @param html The HTML source string to parse.
     * @return Pointer to a heap-allocated Document (ownership transfers to caller).
     * @note The returned pointer must be deleted by the caller (or stored in a
     *       std::unique_ptr<Document>).
     */
    Document* parse(const std::string& html);

    /**
     * @brief Parses HTML into an existing Document, replacing its children.
     * @param doc  The document to populate (its existing children are cleared).
     * @param html The HTML source string to parse.
     */
    void parseInto(Document& doc, const std::string& html);
};

} // namespace html

#endif // HTML_HTMLPARSER_H

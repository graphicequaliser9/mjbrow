/**
 * @file HTMLParser.h
 * @brief HTML5 parser scaffolding.
 * @details HTML5 parser: tokeniser, tree-builder, DOM node types, innerHTML setter.
 *          Doubles as html/Tokenizer.h — the tokeniser lives here for now.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef HTML_HTMLPARSER_H
#define HTML_HTMLPARSER_H

#include <string>
#include <vector>

// --- html/Tokenizer.h ---

namespace html {

enum class TokenType {
    DOCTYPE,
    StartTag,
    EndTag,
    Comment,
    Character,
    EOF_TOKEN,
};

struct Token {
    TokenType type;
    std::string data;        ///< tag name, character data, comment text, etc.
    std::vector<std::pair<std::string, std::string>> attributes; // attr key/value pairs
};

} // namespace html

// --- html/HTMLParser.h ---

namespace html {

class ArenaAllocator; // forward declare

class HTMLParser {
public:
    HTMLParser();
    ~HTMLParser();

    /// @brief Parses HTML input and returns the root document node.
    /// @param html The HTML string to parse.
    /// @return Pointer to the root document node.
    class DOMNode* parse(const std::string& html);

private:
    class DOMNodePool* nodePool_;
};

} // namespace html

#endif // HTML_HTMLPARSER_H
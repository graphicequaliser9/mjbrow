/**
 * @file html/Tokenizer.h
 * @brief HTML5 tokeniser (lexer).
 * @details Produces a flat stream of tokens (DOCTYPE, start tags, end tags,
 *          comments, character data, EOF) consumed by the tree builder.  This
 *          header is the canonical home of html::Token and html::TokenType.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef HTML_TOKENIZER_H
#define HTML_TOKENIZER_H

#include <string>
#include <vector>
#include <cctype>

namespace html {

/**
 * @enum TokenType
 * @brief Discriminator for the kind of token produced by the tokeniser.
 */
enum class TokenType {
    DOCTYPE,     ///< <!DOCTYPE html>
    StartTag,    ///< <tag attr="value">
    EndTag,      ///< </tag>
    Comment,     ///< <!-- ... -->
    Character,   ///< raw character data / text
    EOF_TOKEN,   ///< end of input
};

/**
 * @struct Token
 * @brief A single token emitted by the tokeniser.
 */
struct Token {
    TokenType type{TokenType::EOF_TOKEN};
    std::string data;                                  ///< tag name, comment text, or character data
    std::vector<std::pair<std::string, std::string>> attributes; ///< attribute name/value pairs (start tags)
    bool selfClosing{false};                          ///< true for <br/> style self-closing tags
};

/**
 * @class Tokenizer
 * @brief Converts an HTML byte-string into a flat token stream.
 * @details Implements the HTML5 tokenisation algorithm for the common cases:
 *          start tags (with attribute parsing), end tags, comments, DOCTYPE,
 *          and character data, ending with a single EOF token.  Raw-text
 *          elements (script, style, title, textarea, etc.) have their content
 *          emitted as a single character token so that embedded `<` characters
 *          are preserved verbatim.
 */
class Tokenizer {
public:
    /**
     * @brief Constructs a tokeniser over the given input string.
     * @param input The full HTML source to tokenise (copied).
     */
    explicit Tokenizer(const std::string& input)
        : input_(input) {}

    /**
     * @brief Tokenises the input and returns the full token stream.
     * @return A vector of tokens ending in a single EOF_TOKEN.
     */
    std::vector<Token> tokenize();

private:
    std::string input_;
    size_t pos_{0};

    // --- low-level scanning helpers ---
    bool atEnd() const { return pos_ >= input_.size(); }
    void skipWhitespace();
    std::string toLower(std::string s) const;
    std::string readTagName();
    void readAttributes(Token& tok);
    std::string readCommentText();
    std::string readBogusComment();
    std::string readRawUntil(char c);
    std::string readRawTextContent(const std::string& endTagLower);
    bool isRawTextElement(const std::string& tag) const;
};

// ── inline helpers ─────────────────────────────────────────────────────────

inline void Tokenizer::skipWhitespace() {
    while (!atEnd() && std::isspace(static_cast<unsigned char>(input_[pos_]))) ++pos_;
}

inline std::string Tokenizer::toLower(std::string s) const {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::string Tokenizer::readRawUntil(char /*c*/) {
    size_t start = pos_;
    while (!atEnd() && input_[pos_] != '>') ++pos_;
    std::string s = input_.substr(start, pos_ - start);
    if (!atEnd()) ++pos_;
    return s;
}

inline bool Tokenizer::isRawTextElement(const std::string& tag) const {
    std::string lower = toLower(tag);
    return lower == "script" || lower == "style" || lower == "title" ||
           lower == "textarea" || lower == "noscript" || lower == "xmp" ||
           lower == "iframe" || lower == "noembed" || lower == "noframes" ||
           lower == "plaintext";
}

} // namespace html

#endif // HTML_TOKENIZER_H

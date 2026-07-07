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
 *          and character data, ending with a single EOF token.
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
    std::string readRawUntil(char c);     ///< read until a literal char (for DOCTYPE bogus)
};

// ── inline implementation ─────────────────────────────────────────────────────

inline void Tokenizer::skipWhitespace() {
    while (!atEnd() && std::isspace(static_cast<unsigned char>(input_[pos_]))) ++pos_;
}

inline std::string Tokenizer::toLower(std::string s) const {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::string Tokenizer::readTagName() {
    size_t start = pos_;
    while (!atEnd() &&
           !std::isspace(static_cast<unsigned char>(input_[pos_])) &&
           input_[pos_] != '>' && input_[pos_] != '/') {
        ++pos_;
    }
    return toLower(input_.substr(start, pos_ - start));
}

inline void Tokenizer::readAttributes(Token& tok) {
    while (!atEnd()) {
        skipWhitespace();
        if (atEnd() || input_[pos_] == '>' || input_[pos_] == '/') break;

        size_t nameStart = pos_;
        while (!atEnd() &&
               !std::isspace(static_cast<unsigned char>(input_[pos_])) &&
               input_[pos_] != '=' && input_[pos_] != '>') {
            ++pos_;
        }
        std::string name = toLower(input_.substr(nameStart, pos_ - nameStart));

        skipWhitespace();
        std::string value;
        if (!atEnd() && input_[pos_] == '=') {
            ++pos_;
            skipWhitespace();
            if (!atEnd() && (input_[pos_] == '"' || input_[pos_] == '\'')) {
                char quote = input_[pos_++];
                size_t valStart = pos_;
                while (!atEnd() && input_[pos_] != quote) ++pos_;
                value = input_.substr(valStart, pos_ - valStart);
                if (!atEnd()) ++pos_;
            } else {
                size_t valStart = pos_;
                while (!atEnd() &&
                       !std::isspace(static_cast<unsigned char>(input_[pos_])) &&
                       input_[pos_] != '>') {
                    ++pos_;
                }
                value = input_.substr(valStart, pos_ - valStart);
            }
        }
        tok.attributes.emplace_back(name, value);
    }
}

inline std::string Tokenizer::readCommentText() {
    size_t start = pos_;
    while (pos_ + 2 < input_.size() &&
           !(input_[pos_] == '-' && input_[pos_ + 1] == '-' && input_[pos_ + 2] == '>')) {
        ++pos_;
    }
    std::string comment = input_.substr(start, pos_ - start);
    if (pos_ + 2 < input_.size()) pos_ += 3;
    return comment;
}

inline std::string Tokenizer::readBogusComment() {
    // Used for <!DOCTYPE ...> and other <!...> declarations: read to '>'.
    size_t start = pos_;
    while (!atEnd() && input_[pos_] != '>') ++pos_;
    std::string s = input_.substr(start, pos_ - start);
    if (!atEnd()) ++pos_;
    return s;
}

inline std::string Tokenizer::readRawUntil(char /*c*/) { return readBogusComment(); }

inline std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;

    while (!atEnd()) {
        if (input_[pos_] == '<') {
            ++pos_;
            if (atEnd()) break;

            if (input_[pos_] == '/') {
                // End tag
                ++pos_;
                skipWhitespace();
                std::string tag = readTagName();
                tokens.push_back({TokenType::EndTag, tag, {}});
                while (!atEnd() && input_[pos_] != '>') ++pos_;
                if (!atEnd()) ++pos_;
            } else if (input_[pos_] == '!') {
                ++pos_;
                if (!atEnd() && input_[pos_] == '-' &&
                    pos_ + 1 < input_.size() && input_[pos_ + 1] == '-') {
                    pos_ += 2;
                    std::string comment = readCommentText();
                    tokens.push_back({TokenType::Comment, comment, {}});
                } else {
                    // DOCTYPE or bogus comment
                    readBogusComment();
                    tokens.push_back({TokenType::DOCTYPE, "", {}});
                }
            } else {
                // Start tag
                std::string tag = readTagName();
                Token tok{TokenType::StartTag, tag, {}};
                readAttributes(tok);

                if (!atEnd() && input_[pos_] == '/') {
                    tok.selfClosing = true;
                    ++pos_;
                }
                while (!atEnd() && input_[pos_] != '>') ++pos_;
                if (!atEnd()) ++pos_;
                tokens.push_back(tok);
            }
        } else {
            // Character data
            size_t start = pos_;
            while (!atEnd() && input_[pos_] != '<') ++pos_;
            if (pos_ > start) {
                tokens.push_back({TokenType::Character, input_.substr(start, pos_ - start), {}});
            }
        }
    }

    tokens.push_back({TokenType::EOF_TOKEN, "", {}});
    return tokens;
}

} // namespace html

#endif // HTML_TOKENIZER_H

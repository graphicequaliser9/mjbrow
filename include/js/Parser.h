#ifndef JS_PARSER_H
#define JS_PARSER_H

#include <string>
#include <vector>
#include <memory>
#include "Value.h"
#include "BytecodeCompiler.h"

namespace js {

enum class TokenType {
    EOF_TOKEN,
    IDENTIFIER,
    NUMBER,
    STRING,
    
    // Keywords
    LET,
    CONST,
    VAR,
    IF,
    ELSE,
    FOR,
    WHILE,
    FUNCTION,
    RETURN,
    CLASS,
    NEW,
    THIS,
    SUPER,
    EXTENDS,
    EXPORT,
    IMPORT,
    DEFAULT,
    ASYNC,
    AWAIT,
    TRY,
    CATCH,
    FINALLY,
    THROW,
    SWITCH,
    CASE,
    BREAK,
    CONTINUE,
    DEBUGGER,
    DO,
    IN,
    INSTANCEOF,
    TYPEOF,
    VOID,
    DELETE,
    
    // Arrow function
    ARROW,
    
    // Template literals
    TEMPLATE_START,
    TEMPLATE_MIDDLE,
    TEMPLATE_END,
    
    // Operators
    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,
    EQUAL,
    PLUS_EQUAL,
    MINUS_EQUAL,
    STAR_EQUAL,
    PERCENT_EQUAL,
    EQUAL_EQUAL,
    NOT_EQUAL,
    LESS,
    GREATER,
    LESS_EQUAL,
    GREATER_EQUAL,
    NOT,
    AND,
    OR,
    QUESTION,
    COLON,
    SEMICOLON,
    COMMA,
    DOT,
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACE,
    RIGHT_BRACE,
    LEFT_BRACKET,
    RIGHT_BRACKET,
    
    // Destructuring
    SPREAD
};

struct Token {
    TokenType type;
    std::string text;
    double number_value = 0;
    int line = 1;
    int column = 1;
};

class Lexer {
public:
    Lexer(const std::string& source);
    
    Token nextToken();
    Token peekToken(int ahead = 0);
    
private:
    std::string source_;
    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
    
    void skipWhitespace();
    Token readIdentifier();
    Token readNumber();
    Token readString(char quote);
    Token readTemplateLiteral();
};

class ASTNode {
public:
    enum NodeType {
        PROGRAM,
        BLOCK,
        EXPRESSION_STATEMENT,
        VARIABLE_DECLARATION,
        BINARY_EXPRESSION,
        UNARY_EXPRESSION,
        CALL_EXPRESSION,
        MEMBER_EXPRESSION,
        IDENTIFIER,
        LITERAL,
        FUNCTION_DECLARATION,
        ARROW_FUNCTION_EXPRESSION,
        RETURN_STATEMENT,
        IF_STATEMENT,
        FOR_STATEMENT,
        WHILE_STATEMENT,
        ARRAY_EXPRESSION,
        OBJECT_EXPRESSION,
        TEMPLATE_LITERAL,
        ASSIGNMENT_EXPRESSION,
        ASYNC_FUNCTION_DECLARATION,
        AWAIT_EXPRESSION,
        CLASS_DECLARATION,
        NEW_EXPRESSION
    };
    
    NodeType type;
    std::vector<std::unique_ptr<ASTNode>> children;
    Token token;
    std::string string_value;
    double number_value = 0;
    
    ASTNode(NodeType t) : type(t) {}
};

class Parser {
public:
    Parser();
    
    std::unique_ptr<ASTNode> parse(const std::string& source);
    
private:
    std::unique_ptr<Lexer> lexer_;
    Token current_token_;
    
    void advance();
    bool match(TokenType type);
    void expect(TokenType type);
    
    std::unique_ptr<ASTNode> parseProgram();
    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<ASTNode> parseBlock();
    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parseAssignmentExpression();
    std::unique_ptr<ASTNode> parseConditionalExpression();
    std::unique_ptr<ASTNode> parseLogicalOrExpression();
    std::unique_ptr<ASTNode> parseLogicalAndExpression();
    std::unique_ptr<ASTNode> parseEqualityExpression();
    std::unique_ptr<ASTNode> parseRelationalExpression();
    std::unique_ptr<ASTNode> parseAdditiveExpression();
    std::unique_ptr<ASTNode> parseMultiplicativeExpression();
    std::unique_ptr<ASTNode> parseUnaryExpression();
    std::unique_ptr<ASTNode> parseLeftHandSideExpression();
    std::unique_ptr<ASTNode> parseCallExpression(std::unique_ptr<ASTNode> callee);
    std::unique_ptr<ASTNode> parseMemberExpression();
    std::unique_ptr<ASTNode> parsePrimaryExpression();
    std::unique_ptr<ASTNode> parseArrayExpression();
    std::unique_ptr<ASTNode> parseObjectExpression();
    std::unique_ptr<ASTNode> parseFunctionExpression();
    std::unique_ptr<ASTNode> parseArrowFunctionExpression();
    std::unique_ptr<ASTNode> parseTemplateLiteral();
    std::unique_ptr<ASTNode> parseVariableDeclaration();
    std::unique_ptr<ASTNode> parseIfStatement();
    std::unique_ptr<ASTNode> parseForStatement();
    std::unique_ptr<ASTNode> parseWhileStatement();
};

} // namespace js

#endif // JS_PARSER_H
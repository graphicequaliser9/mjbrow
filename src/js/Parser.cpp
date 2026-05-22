#include "js/Parser.h"
#include <cctype>
#include <cstdlib>

namespace js {

// Lexer implementation
Lexer::Lexer(const std::string& source) : source_(source) {
    advance(); // Initialize current token
}

void Lexer::advance() {
    current_token_ = nextToken();
}

Token Lexer::nextToken() {
    skipWhitespace();
    
    Token token;
    token.line = line_;
    token.column = column_;
    
    if (pos_ >= source_.size()) {
        token.type = TokenType::EOF_TOKEN;
        return token;
    }
    
    char c = source_[pos_];
    
    // Handle template literals
    if (c == '`') {
        return readTemplateLiteral();
    }
    
    // Comments
    if (c == '/') {
        if (pos_ + 1 < source_.size()) {
            char next = source_[pos_ + 1];
            if (next == '/') {
                // Line comment
                while (pos_ < source_.size() && source_[pos_] != '\n') pos_++;
                column_ = 1;
                line_++;
                return nextToken();
            } else if (next == '*') {
                // Block comment
                pos_ += 2;
                while (pos_ + 1 < source_.size()) {
                    if (source_[pos_] == '*' && source_[pos_ + 1] == '/') {
                        pos_ += 2;
                        column_ += 2;
                        break;
                    }
                    if (source_[pos_] == '\n') { line_++; column_ = 1; }
                    else { column_++; }
                    pos_++;
                }
                return nextToken();
            }
        }
    }
    
    // Numbers
    if (std::isdigit(c) || (c == '.' && pos_ + 1 < source_.size() && std::isdigit(source_[pos_ + 1]))) {
        return readNumber();
    }
    
    // Strings
    if (c == '"' || c == '\'') {
        return readString(c);
    }
    
    // Identifiers and keywords
    if (std::isalpha(c) || c == '_' || c == '$') {
        return readIdentifier();
    }
    
    // Operators
    token.text = c;
    column_++;
    pos_++;
    
    switch (c) {
        case '+':
            if (pos_ < source_.size() && source_[pos_] == '=') {
                token.type = TokenType::PLUS_EQUAL;
                token.text = "+=";
                pos_++;
                column_++;
            } else if (pos_ < source_.size() && source_[pos_] == '+') {
                token.type = TokenType::PLUS;
                token.text = "++";
            } else {
                token.type = TokenType::PLUS;
            }
            break;
        case '-':
            token.type = TokenType::MINUS;
            break;
        case '*':
            if (pos_ < source_.size() && source_[pos_] == '=') {
                token.type = TokenType::STAR_EQUAL;
                token.text = "*=";
                pos_++;
                column_++;
            } else {
                token.type = TokenType::STAR;
            }
            break;
        case '%':
            token.type = TokenType::PERCENT;
            break;
        case '=':
            if (pos_ < source_.size() && source_[pos_] == '=') {
                token.type = TokenType::EQUAL_EQUAL;
                token.text = "==";
                pos_++;
                column_++;
            } else if (pos_ < source_.size() && source_[pos_] == '>') {
                token.type = TokenType::ARROW;
                token.text = "=>";
                pos_++;
                column_++;
            } else {
                token.type = TokenType::EQUAL;
            }
            break;
        case '<':
            token.type = TokenType::LESS;
            break;
        case '>':
            token.type = TokenType::GREATER;
            break;
        case '!':
            if (pos_ < source_.size() && source_[pos_] == '=') {
                token.type = TokenType::NOT_EQUAL;
                token.text = "!=";
                pos_++;
                column_++;
            } else {
                token.type = TokenType::NOT;
            }
            break;
        case '&':
            token.type = TokenType::AND;
            break;
        case '|':
            token.type = TokenType::OR;
            break;
        case '?':
            token.type = TokenType::QUESTION;
            break;
        case ':':
            token.type = TokenType::COLON;
            break;
        case ';':
            token.type = TokenType::SEMICOLON;
            break;
        case ',':
            token.type = TokenType::COMMA;
            break;
        case '.':
            token.type = TokenType::DOT;
            break;
        case '(':
            token.type = TokenType::LEFT_PAREN;
            break;
        case ')':
            token.type = TokenType::RIGHT_PAREN;
            break;
        case '{':
            token.type = TokenType::LEFT_BRACE;
            break;
        case '}':
            token.type = TokenType::RIGHT_BRACE;
            break;
        case '[':
            token.type = TokenType::LEFT_BRACKET;
            break;
        case ']':
            token.type = TokenType::RIGHT_BRACKET;
            break;
        default:
            token.type = TokenType::EOF_TOKEN;
    }
    
    return token;
}

void Lexer::skipWhitespace() {
    while (pos_ < source_.size()) {
        char c = source_[pos_];
        if (c == ' ' || c == '\t' || c == '\r') {
            pos_++;
            column_++;
        } else if (c == '\n') {
            pos_++;
            line_++;
            column_ = 1;
        } else {
            break;
        }
    }
}

Token Lexer::readIdentifier() {
    Token token;
    token.line = line_;
    token.column = column_;
    
    size_t start = pos_;
    while (pos_ < source_.size()) {
        char c = source_[pos_];
        if (std::isalnum(c) || c == '_' || c == '$') {
            pos_++;
            column_++;
        } else {
            break;
        }
    }
    
    token.text = source_.substr(start, pos_ - start);
    
    // Keyword lookup
    if (token.text == "let") token.type = TokenType::LET;
    else if (token.text == "const") token.type = TokenType::CONST;
    else if (token.text == "var") token.type = TokenType::VAR;
    else if (token.text == "if") token.type = TokenType::IF;
    else if (token.text == "else") token.type = TokenType::ELSE;
    else if (token.text == "for") token.type = TokenType::FOR;
    else if (token.text == "while") token.type = TokenType::WHILE;
    else if (token.text == "function") token.type = TokenType::FUNCTION;
    else if (token.text == "return") token.type = TokenType::RETURN;
    else if (token.text == "class") token.type = TokenType::CLASS;
    else if (token.text == "new") token.type = TokenType::NEW;
    else if (token.text == "this") token.type = TokenType::THIS;
    else if (token.text == "super") token.type = TokenType::SUPER;
    else if (token.text == "extends") token.type = TokenType::EXTENDS;
    else if (token.text == "export") token.type = TokenType::EXPORT;
    else if (token.text == "import") token.type = TokenType::IMPORT;
    else if (token.text == "default") token.type = TokenType::DEFAULT;
    else if (token.text == "async") token.type = TokenType::ASYNC;
    else if (token.text == "await") token.type = TokenType::AWAIT;
    else if (token.text == "try") token.type = TokenType::TRY;
    else if (token.text == "catch") token.type = TokenType::CATCH;
    else if (token.text == "finally") token.type = TokenType::FINALLY;
    else if (token.text == "throw") token.type = TokenType::THROW;
    else if (token.text == "switch") token.type = TokenType::SWITCH;
    else if (token.text == "case") token.type = TokenType::CASE;
    else if (token.text == "break") token.type = TokenType::BREAK;
    else if (token.text == "continue") token.type = TokenType::CONTINUE;
    else if (token.text == "debugger") token.type = TokenType::DEBUGGER;
    else if (token.text == "do") token.type = TokenType::DO;
    else if (token.text == "in") token.type = TokenType::IN;
    else if (token.text == "instanceof") token.type = TokenType::INSTANCEOF;
    else if (token.text == "typeof") token.type = TokenType::TYPEOF;
    else {
        token.type = TokenType::IDENTIFIER;
    }
    
    return token;
}

Token Lexer::readNumber() {
    Token token;
    token.type = TokenType::NUMBER;
    token.line = line_;
    token.column = column_;
    
    size_t start = pos_;
    bool hasDot = false;
    
    while (pos_ < source_.size()) {
        char c = source_[pos_];
        if (std::isdigit(c)) {
            pos_++;
            column_++;
        } else if (c == '.' && !hasDot) {
            hasDot = true;
            pos_++;
            column_++;
        } else {
            break;
        }
    }
    
    token.text = source_.substr(start, pos_ - start);
    token.number_value = std::stod(token.text);
    
    return token;
}

Token Lexer::readString(char quote) {
    Token token;
    token.type = TokenType::STRING;
    token.line = line_;
    token.column = column_;
    
    pos_++; // Skip opening quote
    column_++;
    
    size_t start = pos_;
    while (pos_ < source_.size()) {
        char c = source_[pos_];
        if (c == '\\' && pos_ + 1 < source_.size()) {
            pos_ += 2;
            column_ += 2;
            continue;
        }
        if (c == quote) {
            break;
        }
        pos_++;
        column_++;
    }
    
    token.text = source_.substr(start, pos_ - start);
    
    if (pos_ < source_.size()) {
        pos_++; // Skip closing quote
        column_++;
    }
    
    return token;
}

Token Lexer::readTemplateLiteral() {
    Token token;
    token.type = TokenType::TEMPLATE_START;
    token.line = line_;
    token.column = column_;
    
    pos_++; // Skip opening backtick
    column_++;
    
    std::string content;
    while (pos_ < source_.size()) {
        char c = source_[pos_];
        if (c == '`') {
            token.type = TokenType::TEMPLATE_END;
            pos_++;
            column_++;
            break;
        }
        if (c == '$' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '{') {
            token.type = TokenType::TEMPLATE_MIDDLE;
            pos_ += 2;
            column_ += 2;
            token.text = content;
            return token;
        }
        content += c;
        pos_++;
        column_++;
    }
    
    token.text = content;
    return token;
}

Token Lexer::peekToken(int ahead) {
    // Simplified - would need to save/restore state for full implementation
    return current_token_;
}

// Parser implementation
Parser::Parser() {}

std::unique_ptr<ASTNode> Parser::parse(const std::string& source) {
    lexer_ = std::make_unique<Lexer>(source);
    current_token_ = lexer_->nextToken();
    return parseProgram();
}

void Parser::advance() {
    current_token_ = lexer_->nextToken();
}

bool Parser::match(TokenType type) {
    return current_token_.type == type;
}

void Parser::expect(TokenType type) {
    if (!match(type)) {
        // Error recovery
    }
    advance();
}

std::unique_ptr<ASTNode> Parser::parseProgram() {
    auto program = std::make_unique<ASTNode>(ASTNode::PROGRAM);
    
    while (!match(TokenType::EOF_TOKEN)) {
        program->children.push_back(parseStatement());
    }
    
    return program;
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    if (match(TokenType::LET) || match(TokenType::CONST) || match(TokenType::VAR)) {
        return parseVariableDeclaration();
    }
    if (match(TokenType::IF)) {
        return parseIfStatement();
    }
    if (match(TokenType::FOR)) {
        return parseForStatement();
    }
    if (match(TokenType::WHILE)) {
        return parseWhileStatement();
    }
    if (match(TokenType::FUNCTION)) {
        return parseFunctionExpression();
    }
    if (match(TokenType::RETURN)) {
        auto ret = std::make_unique<ASTNode>(ASTNode::RETURN_STATEMENT);
        advance();
        if (!match(TokenType::SEMICOLON)) {
            ret->children.push_back(parseExpression());
        }
        if (match(TokenType::SEMICOLON)) advance();
        return ret;
    }
    if (match(TokenType::LEFT_BRACE)) {
        return parseBlock();
    }
    
    // Expression statement
    auto expr = parseExpression();
    if (match(TokenType::SEMICOLON)) advance();
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseBlock() {
    auto block = std::make_unique<ASTNode>(ASTNode::BLOCK);
    expect(TokenType::LEFT_BRACE);
    
    while (!match(TokenType::RIGHT_BRACE) && !match(TokenType::EOF_TOKEN)) {
        block->children.push_back(parseStatement());
    }
    
    expect(TokenType::RIGHT_BRACE);
    return block;
}

std::unique_ptr<ASTNode> Parser::parseExpression() {
    return parseAssignmentExpression();
}

std::unique_ptr<ASTNode> Parser::parseAssignmentExpression() {
    auto left = parseConditionalExpression();
    
    if (match(TokenType::EQUAL) || match(TokenType::PLUS_EQUAL) || 
        match(TokenType::MINUS_EQUAL) || match(TokenType::STAR_EQUAL) ||
        match(TokenType::PERCENT_EQUAL)) {
        
        auto assign = std::make_unique<ASTNode>(ASTNode::ASSIGNMENT_EXPRESSION);
        assign->token = current_token_;
        advance();
        assign->children.push_back(std::move(left));
        assign->children.push_back(parseAssignmentExpression());
        return assign;
    }
    
    return left;
}

std::unique_ptr<ASTNode> Parser::parseConditionalExpression() {
    auto test = parseLogicalOrExpression();
    
    if (match(TokenType::QUESTION)) {
        advance();
        auto consequent = parseExpression();
        expect(TokenType::COLON);
        auto alternate = parseExpression();
        
        auto cond = std::make_unique<ASTNode>(ASTNode::BINARY_EXPRESSION);
        cond->token = {TokenType::QUESTION};
        cond->children.push_back(std::move(test));
        cond->children.push_back(std::move(consequent));
        cond->children.push_back(std::move(alternate));
        return cond;
    }
    
    return test;
}

std::unique_ptr<ASTNode> Parser::parseLogicalOrExpression() {
    auto left = parseLogicalAndExpression();
    
    while (match(TokenType::OR)) {
        auto expr = std::make_unique<ASTNode>(ASTNode::BINARY_EXPRESSION);
        expr->token = current_token_;
        advance();
        expr->children.push_back(std::move(left));
        expr->children.push_back(parseLogicalAndExpression());
        left = std::move(expr);
    }
    
    return left;
}

std::unique_ptr<ASTNode> Parser::parseLogicalAndExpression() {
    auto left = parseEqualityExpression();
    
    while (match(TokenType::AND)) {
        auto expr = std::make_unique<ASTNode>(ASTNode::BINARY_EXPRESSION);
        expr->token = current_token_;
        advance();
        expr->children.push_back(std::move(left));
        expr->children.push_back(parseEqualityExpression());
        left = std::move(expr);
    }
    
    return left;
}

std::unique_ptr<ASTNode> Parser::parseEqualityExpression() {
    auto left = parseRelationalExpression();
    
    while (match(TokenType::EQUAL_EQUAL) || match(TokenType::NOT_EQUAL)) {
        auto expr = std::make_unique<ASTNode>(ASTNode::BINARY_EXPRESSION);
        expr->token = current_token_;
        advance();
        expr->children.push_back(std::move(left));
        expr->children.push_back(parseRelationalExpression());
        left = std::move(expr);
    }
    
    return left;
}

std::unique_ptr<ASTNode> Parser::parseRelationalExpression() {
    auto left = parseAdditiveExpression();
    
    while (match(TokenType::LESS) || match(TokenType::GREATER) ||
           match(TokenType::LESS_EQUAL) || match(TokenType::GREATER_EQUAL)) {
        auto expr = std::make_unique<ASTNode>(ASTNode::BINARY_EXPRESSION);
        expr->token = current_token_;
        advance();
        expr->children.push_back(std::move(left));
        expr->children.push_back(parseAdditiveExpression());
        left = std::move(expr);
    }
    
    return left;
}

std::unique_ptr<ASTNode> Parser::parseAdditiveExpression() {
    auto left = parseMultiplicativeExpression();
    
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        auto expr = std::make_unique<ASTNode>(ASTNode::BINARY_EXPRESSION);
        expr->token = current_token_;
        advance();
        expr->children.push_back(std::move(left));
        expr->children.push_back(parseMultiplicativeExpression());
        left = std::move(expr);
    }
    
    return left;
}

std::unique_ptr<ASTNode> Parser::parseMultiplicativeExpression() {
    auto left = parseUnaryExpression();
    
    while (match(TokenType::STAR) || match(TokenType::PERCENT) || match(TokenType::DIV)) {
        auto expr = std::make_unique<ASTNode>(ASTNode::BINARY_EXPRESSION);
        expr->token = current_token_;
        advance();
        expr->children.push_back(std::move(left));
        expr->children.push_back(parseUnaryExpression());
        left = std::move(expr);
    }
    
    return left;
}

std::unique_ptr<ASTNode> Parser::parseUnaryExpression() {
    if (match(TokenType::NOT) || match(TokenType::MINUS)) {
        auto expr = std::make_unique<ASTNode>(ASTNode::UNARY_EXPRESSION);
        expr->token = current_token_;
        advance();
        expr->children.push_back(parseUnaryExpression());
        return expr;
    }
    return parseLeftHandSideExpression();
}

std::unique_ptr<ASTNode> Parser::parseLeftHandSideExpression() {
    return parseCallExpression(parseMemberExpression());
}

std::unique_ptr<ASTNode> Parser::parseCallExpression(std::unique_ptr<ASTNode> callee) {
    while (match(TokenType::LEFT_PAREN)) {
        auto call = std::make_unique<ASTNode>(ASTNode::CALL_EXPRESSION);
        call->children.push_back(std::move(callee));
        
        advance(); // skip (
        while (!match(TokenType::RIGHT_PAREN)) {
            call->children.push_back(parseExpression());
            if (match(TokenType::COMMA)) advance();
        }
        advance(); // skip )
        
        callee = std::move(call);
    }
    return callee;
}

std::unique_ptr<ASTNode> Parser::parseMemberExpression() {
    auto object = parsePrimaryExpression();
    
    while (match(TokenType::DOT) || match(TokenType::LEFT_BRACKET)) {
        auto member = std::make_unique<ASTNode>(ASTNode::MEMBER_EXPRESSION);
        member->children.push_back(std::move(object));
        
        if (match(TokenType::DOT)) {
            advance();
            if (match(TokenType::IDENTIFIER)) {
                member->string_value = current_token_.text;
                advance();
            }
        } else {
            advance(); // [
            member->children.push_back(parseExpression());
            expect(TokenType::RIGHT_BRACKET);
        }
        
        object = std::move(member);
    }
    
    return object;
}

std::unique_ptr<ASTNode> Parser::parsePrimaryExpression() {
    auto node = std::make_unique<ASTNode>(ASTNode::LITERAL);
    node->token = current_token_;
    
    if (match(TokenType::NUMBER)) {
        node->number_value = current_token_.number_value;
        advance();
    } else if (match(TokenType::STRING)) {
        node->string_value = current_token_.text;
        advance();
    } else if (match(TokenType::IDENTIFIER)) {
        node->string_value = current_token_.text;
        advance();
    } else if (match(TokenType::LEFT_PAREN)) {
        advance();
        node = parseExpression();
        expect(TokenType::RIGHT_PAREN);
    } else if (match(TokenType::LEFT_BRACKET)) {
        return parseArrayExpression();
    } else if (match(TokenType::LEFT_BRACE)) {
        return parseObjectExpression();
    } else if (match(TokenType::FUNCTION)) {
        return parseFunctionExpression();
    }
    
    return node;
}

std::unique_ptr<ASTNode> Parser::parseArrayExpression() {
    auto arr = std::make_unique<ASTNode>(ASTNode::ARRAY_EXPRESSION);
    advance(); // [
    
    if (!match(TokenType::RIGHT_BRACKET)) {
        do {
            if (match(TokenType::COMMA)) {
                arr->children.push_back(std::make_unique<ASTNode>(ASTNode::LITERAL)); // hole
            } else {
                arr->children.push_back(parseExpression());
            }
        } while (match(TokenType::COMMA) && (advance(), true));
    }
    
    expect(TokenType::RIGHT_BRACKET);
    return arr;
}

std::unique_ptr<ASTNode> Parser::parseObjectExpression() {
    auto obj = std::make_unique<ASTNode>(ASTNode::OBJECT_EXPRESSION);
    advance(); // {
    
    if (!match(TokenType::RIGHT_BRACE)) {
        do {
            std::string key;
            if (match(TokenType::IDENTIFIER) || match(TokenType::STRING) || match(TokenType::NUMBER)) {
                key = current_token_.text;
                advance();
            } else if (match(TokenType::SPREAD)) {
                // Handle spread
                advance();
                continue;
            }
            
            if (match(TokenType::COLON)) {
                advance();
                obj->children.push_back(parseExpression());
            }
        } while (match(TokenType::COMMA) && (advance(), true));
    }
    
    expect(TokenType::RIGHT_BRACE);
    return obj;
}

std::unique_ptr<ASTNode> Parser::parseFunctionExpression() {
    auto func = std::make_unique<ASTNode>(ASTNode::FUNCTION_DECLARATION);
    advance(); // function
    
    if (match(TokenType::IDENTIFIER)) {
        func->string_value = current_token_.text;
        advance();
    }
    
    expect(TokenType::LEFT_PAREN);
    // Parse parameters
    while (!match(TokenType::RIGHT_PAREN)) {
        if (match(TokenType::IDENTIFIER)) {
            func->children.push_back(std::make_unique<ASTNode>(ASTNode::IDENTIFIER));
            func->children.back()->string_value = current_token_.text;
            advance();
        }
        if (match(TokenType::COMMA)) advance();
    }
    expect(TokenType::RIGHT_PAREN);
    
    func->children.push_back(parseBlock());
    return func;
}

std::unique_ptr<ASTNode> Parser::parseVariableDeclaration() {
    auto var = std::make_unique<ASTNode>(ASTNode::VARIABLE_DECLARATION);
    var->token = current_token_;
    advance(); // let/const/var
    
    while (match(TokenType::IDENTIFIER)) {
        auto decl = std::make_unique<ASTNode>(ASTNode::IDENTIFIER);
        decl->string_value = current_token_.text;
        
        advance();
        if (match(TokenType::EQUAL)) {
            advance();
            decl->children.push_back(parseExpression());
        }
        
        var->children.push_back(std::move(decl));
        
        if (match(TokenType::COMMA)) advance();
    }
    
    if (match(TokenType::SEMICOLON)) advance();
    return var;
}

std::unique_ptr<ASTNode> Parser::parseIfStatement() {
    auto ifStmt = std::make_unique<ASTNode>(ASTNode::IF_STATEMENT);
    advance(); // if
    
    expect(TokenType::LEFT_PAREN);
    ifStmt->children.push_back(parseExpression());
    expect(TokenType::RIGHT_PAREN);
    
    ifStmt->children.push_back(parseStatement());
    
    if (match(TokenType::ELSE)) {
        advance(); // else
        ifStmt->children.push_back(parseStatement());
    }
    
    return ifStmt;
}

std::unique_ptr<ASTNode> Parser::parseForStatement() {
    auto forStmt = std::make_unique<ASTNode>(ASTNode::FOR_STATEMENT);
    advance(); // for
    
    expect(TokenType::LEFT_PAREN);
    // init
    if (!match(TokenType::SEMICOLON)) {
        forStmt->children.push_back(parseStatement());
    } else {
        advance();
    }
    // test
    if (!match(TokenType::SEMICOLON)) {
        forStmt->children.push_back(parseExpression());
    }
    expect(TokenType::SEMICOLON);
    // update
    if (!match(TokenType::RIGHT_PAREN)) {
        forStmt->children.push_back(parseExpression());
    }
    expect(TokenType::RIGHT_PAREN);
    
    forStmt->children.push_back(parseStatement());
    return forStmt;
}

std::unique_ptr<ASTNode> Parser::parseWhileStatement() {
    auto whileStmt = std::make_unique<ASTNode>(ASTNode::WHILE_STATEMENT);
    advance(); // while
    
    expect(TokenType::LEFT_PAREN);
    whileStmt->children.push_back(parseExpression());
    expect(TokenType::RIGHT_PAREN);
    
    whileStmt->children.push_back(parseStatement());
    return whileStmt;
}

} // namespace js
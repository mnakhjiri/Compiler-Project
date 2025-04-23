#include "lexer.h"
#include <cctype>

Lexer::Lexer(llvm::StringRef buffer) {
    bufferStart = buffer.begin();
    bufferPtr = bufferStart;
}

void Lexer::skipWhitespace() {
    while (isspace(*bufferPtr)) {
        if (*bufferPtr == '\n') bufferPtr++;
        else bufferPtr++;
    }
}

void Lexer::skipComment() {
    if (*bufferPtr == '/' && *(bufferPtr + 1) == '*') {
        bufferPtr += 2;
        while (!(*bufferPtr == '*' && *(bufferPtr + 1) == '/')) bufferPtr++;
        bufferPtr += 2;
    }
}

Token Lexer::nextToken() {
    skipWhitespace();
    skipComment();

    if (bufferPtr >= bufferStart + buffer.size()) 
        return {Token::eof, llvm::StringRef(), 0, 0};

    const char *tokStart = bufferPtr;
    
    // KWords & Identifiers
    if (isalpha(*bufferPtr)) {
        while (isalnum(*bufferPtr) || *bufferPtr == '_') bufferPtr++;
        llvm::StringRef text(tokStart, bufferPtr - tokStart);
        
        Token::TokenKind kind = Token::identifier;
        if (text == "int") kind = Token::KW_int;
        else if (text == "bool") kind = Token::KW_bool;
        else if (text == "float") kind = Token::KW_float;
        else if (text == "char") kind = Token::KW_char;
        else if (text == "string") kind = Token::KW_string;
        else if (text == "array") kind = Token::KW_array;
        else if (text == "if") kind = Token::KW_if;
        else if (text == "else") kind = Token::KW_else;
        else if (text == "while") kind = Token::KW_while;
        else if (text == "for") kind = Token::KW_for;
        else if (text == "foreach") kind = Token::KW_foreach;
        else if (text == "in") kind = Token::KW_in;
        else if (text == "print") kind = Token::KW_print;
        else if (text == "true") kind = Token::KW_true;
        else if (text == "false") kind = Token::KW_false;
        else if (text == "try") kind = Token::KW_try;
        else if (text == "catch") kind = Token::KW_catch;
        else if (text == "error") kind = Token::KW_error;
        else if (text == "match") kind = Token::KW_match;
        else if (text == "concat") kind = Token::KW_concat;
        else if (text == "pow") kind = Token::KW_pow;
        else if (text == "abs") kind = Token::KW_abs;
        else if (text == "length") kind = Token::KW_length;
        else if (text == "min") kind = Token::KW_min;
        else if (text == "max") kind = Token::KW_max;
        else if (text == "and")   kind = Token::and_op;
        else if (text == "or")    kind = Token::or_op;
        else if (text == "not")   kind = Token::not_op;
        else if (text == "index") kind = Token::KW_index;
        
        return {kind, text, 0, 0};
    }
    
    // numbers
    if (isdigit(*bufferPtr) || *bufferPtr == '.') {
        bool hasDot = false;
    if (*bufferPtr == '.') {
        hasDot = true;
        bufferPtr++;
    }
    while (isdigit(*bufferPtr) || (!hasDot && *bufferPtr == '.')) {
        if (*bufferPtr == '.') hasDot = true;
        bufferPtr++;
    }
    return {hasDot ? Token::float_literal : Token::number, 
            llvm::StringRef(tokStart, bufferPtr - tokStart), 0, 0};}

    // strings
    if (*bufferPtr == '"') {
        bufferPtr++;
        const char *start = bufferPtr;
        while (*bufferPtr != '"' && *bufferPtr != '\0') bufferPtr++;
        return {Token::string_literal, llvm::StringRef(start, bufferPtr - start), 0, 0};
    }
    
    // charachters
    if (*bufferPtr == '\'') {
        bufferPtr++;
        char c = *bufferPtr;
        bufferPtr += 2; // Skip closing '
        return {Token::char_literal, llvm::StringRef(&c, 1), 0, 0};
    }
    
    // operators & symbols
    switch (*bufferPtr) {
        case ';': bufferPtr++; return {Token::semi_colon, ";", 0, 0};
        case ',': bufferPtr++; return {Token::comma, ",", 0, 0};
        case '(': bufferPtr++; return {Token::l_paren, "(", 0, 0};
        case ')': bufferPtr++; return {Token::r_paren, ")", 0, 0};
        case '{': bufferPtr++; return {Token::l_brace, "{", 0, 0};
        case '}': bufferPtr++; return {Token::r_brace, "}", 0, 0};
        case '[': bufferPtr++; return {Token::l_bracket, "[", 0, 0};
        case ']': bufferPtr++; return {Token::r_bracket, "]", 0, 0};
        case '+':
            if (*(bufferPtr + 1) == '+') {
                bufferPtr += 2;
                return {Token::plus_plus, "++", 0, 0};
            } else if (*(bufferPtr + 1) == '=') {
                bufferPtr += 2;
                return {Token::plus_equal, "+=", 0, 0};
            }
            bufferPtr++;
            return {Token::plus, "+", 0, 0};
        case '-':
            if (*(bufferPtr + 1) == '-') {
                bufferPtr += 2;
                return {Token::minus_minus, "--", 0, 0};
            } else if (*(bufferPtr + 1) == '=') {
                bufferPtr += 2;
                return {Token::minus_equal, "-=", 0, 0};
            } else if (*(bufferPtr + 1) == '>') {
                bufferPtr += 2;
                return {Token::arrow, "->", 0, 0};
            }
            bufferPtr++;
            return {Token::minus, "-", 0, 0};
        case '*':
            if (*(bufferPtr + 1) == '=') {
                bufferPtr += 2;
                return {Token::star_equal, "*=", 0, 0};
            }
            bufferPtr++;
            return {Token::star, "*", 0, 0};
        case '/':
            if (*(bufferPtr + 1) == '=') {
                bufferPtr += 2;
                return {Token::slash_equal, "/=", 0, 0};
            }
            bufferPtr++;
            return {Token::slash, "/", 0, 0};
        case '%':
            if (*(bufferPtr + 1) == '=') {
                bufferPtr += 2;
                return {Token::mod_equal, "%=", 0, 0};
            }
            bufferPtr++;
            return {Token::mod, "%", 0, 0};
        case '^':
            bufferPtr++;
            return {Token::caret, "^", 0, 0};
        case '=':
            if (*(bufferPtr + 1) == '=') {
                bufferPtr += 2;
                return {Token::equal_equal, "==", 0, 0};
            }
            bufferPtr++;
            return {Token::equal, "=", 0, 0};
        case '!':
            if (*(bufferPtr + 1) == '=') {
                bufferPtr += 2;
                return {Token::not_equal, "!=", 0, 0};
            } else {
                bufferPtr++;
                return {Token::not_op, "!", 0, 0};
            }
        case '<':
            if (*(bufferPtr + 1) == '=') {
                bufferPtr += 2;
                return {Token::less_equal, "<=", 0, 0};
            }
            bufferPtr++;
            return {Token::less, "<", 0, 0};
        case '>':
            if (*(bufferPtr + 1) == '=') {
                bufferPtr += 2;
                return {Token::greater_equal, ">=", 0, 0};
            }
            bufferPtr++;
            return {Token::greater, ">", 0, 0};
        case '_':
            bufferPtr++;
            return {Token::underscore, "_", 0, 0};
    }
    
    return {Token::eof, llvm::StringRef(), 0, 0};
}

#ifndef LEXER_H
#define LEXER_H

#include "llvm/ADT/StringRef.h"
#include <string>

class Lexer;

class Token {
public:
    enum TokenKind {

        semi_colon,
        identifier,
        number,
        comma,
        l_paren,
        r_paren,
        l_brace,
        r_brace,
        l_bracket,
        r_bracket,
        eof,

        plus,
        minus,
        star,
        slash,
        mod,
        caret,
        equal,
        plus_equal,
        minus_equal,
        star_equal,
        slash_equal,
        mod_equal,
        equal_equal,
        not_equal,
        less,
        less_equal,
        greater,
        greater_equal,
        and_op,
        or_op,
        not_op
        plus_plus,
        minus_minus,


        KW_int,
        KW_bool,
        KW_float,
        KW_char,
        KW_string,
        KW_array,
        KW_if,
        KW_else,
        KW_while,
        KW_for,
        KW_foreach,
        KW_in,
        KW_print,
        KW_true,
        KW_false,
        KW_try,
        KW_catch,
        KW_error,
        KW_match,
        KW_concat,
        KW_pow,
        KW_abs,
        KW_length,
        KW_min,
        KW_max,
        KW_index,

        comment_start,
        comment_end,
        string_literal,
        char_literal,
        float_literal,
        underscore,
        arrow
    };

    TokenKind kind;
    llvm::StringRef text;
    int line;
    int column;

    bool is(TokenKind k) const { return kind == k; }
    bool isOneOf(TokenKind k1, TokenKind k2) const { return is(k1) || is(k2); }
    template <typename... Ts> bool isOneOf(TokenKind k1, TokenKind k2, Ts... ks) const {
        return is(k1) || isOneOf(k2, ks...);
    }
};

class Lexer {
    const char *bufferStart;
    const char *bufferPtr;

public:
    Lexer(llvm::StringRef buffer);
    Token nextToken();

private:
    void skipWhitespace();
    void skipComment();
    Token formToken(Token::TokenKind kind, const char *tokEnd);
};

#endif

#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
    TOKEN_IDENTIFIER,
    TOKEN_CONSTANT,
    TOKEN_KEYWORD_INT,
    TOKEN_KEYWORD_VOID,
    TOKEN_KEYWORD_RETURN,
    TOKEN_OPEN_PAREN,
    TOKEN_CLOSE_PAREN,
    TOKEN_OPEN_BRACE,
    TOKEN_CLOSE_BRACE,
    TOKEN_SEMICOLON,
    TOKEN_TILDE,
    TOKEN_NEGATION,
    TOKEN_DECREMENT,
    TOKEN_PLUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_EOF,
} LexTokenType;

typedef struct {
    LexTokenType type;
    char *value;
    size_t start;   // byte offset in input
    size_t length;  // length of lexeme
} Token;

typedef struct {
    const char *input;
    size_t position;
} Lexer;

void lexer_init(Lexer *lexer, const char *source);
Token lexer_next_token(Lexer *lexer);
void free_token(Token token);

#endif

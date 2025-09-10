#include "../../include/lexer/lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/util/diag.h"

#ifdef _WIN32
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

static const char *keywords[] = {"int", "void", "return"};
static const LexTokenType keyword_tokens[] = {TOKEN_KEYWORD_INT, TOKEN_KEYWORD_VOID, TOKEN_KEYWORD_RETURN};

void lexer_init(Lexer *lexer, const char *source) {
    lexer->input = source;
    lexer->position = 0;
}

static void skip_whitespace(Lexer *lexer) {
    while (isspace((unsigned char)lexer->input[lexer->position])) {
        lexer->position++;
    }
}

static int is_identifier_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_identifier_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static int is_digit(char c) {
    return isdigit((unsigned char)c);
}

static Token make_token(LexTokenType type, const char *start_ptr, size_t length, size_t start_off) {
    Token token;
    token.type = type;
    token.value = (char *)malloc(length + 1);
    memcpy(token.value, start_ptr, length);
    token.value[length] = '\0';
    token.start = start_off;
    token.length = length;
    return token;
}

static Token match_identifier_or_keyword(Lexer *lexer) {
    size_t start_pos = lexer->position;
    while (is_identifier_char(lexer->input[lexer->position])) {
        lexer->position++;
    }
    size_t length = lexer->position - start_pos;
    
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (strncmp(lexer->input + start_pos, keywords[i], length) == 0 && strlen(keywords[i]) == length) {
            return make_token(keyword_tokens[i], lexer->input + start_pos, length, start_pos);
        }
    }

    return make_token(TOKEN_IDENTIFIER, lexer->input + start_pos, length, start_pos);
}

static Token match_constant(Lexer *lexer) {
    size_t start_pos = lexer->position;
    while (is_digit(lexer->input[lexer->position])) {
        lexer->position++;
    }
    return make_token(TOKEN_CONSTANT, lexer->input + start_pos, lexer->position - start_pos, start_pos);
}

Token lexer_next_token(Lexer *lexer) {
    skip_whitespace(lexer);

    if (lexer->input[lexer->position] == '\0') {
        return make_token(TOKEN_EOF, "", 0, lexer->position);
    }

    char c = lexer->input[lexer->position];

    if (is_identifier_start(c)) {
        return match_identifier_or_keyword(lexer);
    }

    if (is_digit(c)) {
        return match_constant(lexer);
    }

    lexer->position++;

    switch (c) {
        case '(': return make_token(TOKEN_OPEN_PAREN, "(", 1, lexer->position - 1);
        case ')': return make_token(TOKEN_CLOSE_PAREN, ")", 1, lexer->position - 1);
        case '{': return make_token(TOKEN_OPEN_BRACE, "{", 1, lexer->position - 1);
        case '}': return make_token(TOKEN_CLOSE_BRACE, "}", 1, lexer->position - 1);
        case ';': return make_token(TOKEN_SEMICOLON, ";", 1, lexer->position - 1);
        case '~': return make_token(TOKEN_TILDE, "~", 1, lexer->position - 1);
        case '+': return make_token(TOKEN_PLUS, "+", 1, lexer->position - 1);
        case '*': return make_token(TOKEN_STAR, "*", 1, lexer->position - 1);
        case '/': return make_token(TOKEN_SLASH, "/", 1, lexer->position - 1);
        case '%': return make_token(TOKEN_PERCENT, "%", 1, lexer->position - 1);
        case '-':
            if (lexer->input[lexer->position] == '-') {
                lexer->position++;
                return make_token(TOKEN_DECREMENT, "--", 2, lexer->position - 2);
            }
            return make_token(TOKEN_NEGATION, "-", 1, lexer->position - 1);
        default: {
            size_t pos = lexer->position - 1;
            int line = 0, col = 0;
            compute_line_col(lexer->input, pos, &line, &col);
            fprintf(stderr, "Lexer Error at %d:%d: Invalid token '%c'\n", line, col, c);
            exit(1);
        }
    }
}

void free_token(Token token) {
    free(token.value);
}

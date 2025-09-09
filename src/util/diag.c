#include "../../include/util/diag.h"
#include <string.h>

void compute_line_col(const char *src, size_t pos, int *out_line, int *out_col) {
    int line = 1, col = 1;
    for (size_t i = 0; i < pos; i++) {
        if (src[i] == '\n') { line++; col = 1; }
        else { col++; }
    }
    if (out_line) *out_line = line;
    if (out_col) *out_col = col;
}

const char *token_type_name(LexTokenType t) {
    switch (t) {
        case TOKEN_IDENTIFIER: return "TOKEN_IDENTIFIER";
        case TOKEN_CONSTANT: return "TOKEN_CONSTANT";
        case TOKEN_KEYWORD_INT: return "TOKEN_KEYWORD_INT";
        case TOKEN_KEYWORD_VOID: return "TOKEN_KEYWORD_VOID";
        case TOKEN_KEYWORD_RETURN: return "TOKEN_KEYWORD_RETURN";
        case TOKEN_OPEN_PAREN: return "TOKEN_OPEN_PAREN";
        case TOKEN_CLOSE_PAREN: return "TOKEN_CLOSE_PAREN";
        case TOKEN_OPEN_BRACE: return "TOKEN_OPEN_BRACE";
        case TOKEN_CLOSE_BRACE: return "TOKEN_CLOSE_BRACE";
        case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON";
        case TOKEN_TILDE: return "TOKEN_TILDE";
        case TOKEN_NEGATION: return "TOKEN_NEGATION";
        case TOKEN_DECREMENT: return "TOKEN_DECREMENT";
        case TOKEN_EOF: return "TOKEN_EOF";
        default: return "TOKEN_UNKNOWN";
    }
}


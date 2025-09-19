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
        case TOKEN_KEYWORD_IF: return "TOKEN_KEYWORD_IF";
        case TOKEN_KEYWORD_ELSE: return "TOKEN_KEYWORD_ELSE";
        case TOKEN_KEYWORD_DO: return "TOKEN_KEYWORD_DO";
        case TOKEN_KEYWORD_WHILE: return "TOKEN_KEYWORD_WHILE";
        case TOKEN_KEYWORD_FOR: return "TOKEN_KEYWORD_FOR";
        case TOKEN_KEYWORD_BREAK: return "TOKEN_KEYWORD_BREAK";
        case TOKEN_KEYWORD_CONTINUE: return "TOKEN_KEYWORD_CONTINUE";
        case TOKEN_OPEN_PAREN: return "TOKEN_OPEN_PAREN";
        case TOKEN_CLOSE_PAREN: return "TOKEN_CLOSE_PAREN";
        case TOKEN_OPEN_BRACE: return "TOKEN_OPEN_BRACE";
        case TOKEN_CLOSE_BRACE: return "TOKEN_CLOSE_BRACE";
        case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON";
        case TOKEN_QUESTION: return "TOKEN_QUESTION";
        case TOKEN_COLON: return "TOKEN_COLON";
        case TOKEN_TILDE: return "TOKEN_TILDE";
        case TOKEN_NOT: return "TOKEN_NOT";
        case TOKEN_NEGATION: return "TOKEN_NEGATION";
        case TOKEN_DECREMENT: return "TOKEN_DECREMENT";
        case TOKEN_ASSIGN: return "TOKEN_ASSIGN";
        case TOKEN_PLUS: return "TOKEN_PLUS";
        case TOKEN_STAR: return "TOKEN_STAR";
        case TOKEN_SLASH: return "TOKEN_SLASH";
        case TOKEN_PERCENT: return "TOKEN_PERCENT";
        case TOKEN_AMP_AMP: return "TOKEN_AMP_AMP";
        case TOKEN_PIPE_PIPE: return "TOKEN_PIPE_PIPE";
        case TOKEN_EQUAL_EQUAL: return "TOKEN_EQUAL_EQUAL";
        case TOKEN_NOT_EQUAL: return "TOKEN_NOT_EQUAL";
        case TOKEN_LESS: return "TOKEN_LESS";
        case TOKEN_LESS_EQUAL: return "TOKEN_LESS_EQUAL";
        case TOKEN_GREATER: return "TOKEN_GREATER";
        case TOKEN_GREATER_EQUAL: return "TOKEN_GREATER_EQUAL";
        case TOKEN_EOF: return "TOKEN_EOF";
        default: return "TOKEN_UNKNOWN";
    }
}

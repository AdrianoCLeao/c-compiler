#ifndef UTIL_DIAG_H
#define UTIL_DIAG_H

#include <stddef.h>
#include "../lexer/lexer.h"

void compute_line_col(const char *src, size_t pos, int *out_line, int *out_col);

const char *token_type_name(LexTokenType t);

#endif


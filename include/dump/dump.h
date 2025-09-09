#ifndef DUMP_H
#define DUMP_H

#include <stdbool.h>
#include "../parser/parser.h"

typedef enum {
    DUMP_AST_NONE = 0,
    DUMP_AST_TXT,
    DUMP_AST_DOT,
    DUMP_AST_JSON,
} DumpAstFormat;

void dump_ensure_out_dir(void);

char *dump_default_path(const char *input_path, const char *ext);

bool dump_tokens_file(const char *input_path, const char *source, const char *out_path);

bool dump_ast_file(ASTNode *ast, const char *input_path, DumpAstFormat fmt, const char *out_path);

#endif


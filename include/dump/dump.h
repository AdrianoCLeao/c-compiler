#ifndef DUMP_H
#define DUMP_H

#include <stdbool.h>
#include "../parser/parser.h"
#include "../tacky/tacky.h"

typedef enum {
    DUMP_AST_NONE = 0,
    DUMP_AST_TXT,
    DUMP_AST_DOT,
    DUMP_AST_JSON,
} DumpAstFormat;

typedef enum {
    DUMP_TACKY_NONE = 0,
    DUMP_TACKY_TXT,
    DUMP_TACKY_JSON,
} DumpTackyFormat;

void dump_ensure_out_dir(void);

char *dump_default_path(const char *input_path, const char *ext);

bool dump_tokens_file(const char *input_path, const char *source, const char *out_path);

bool dump_ast_file(ASTNode *ast, const char *input_path, DumpAstFormat fmt, const char *out_path);

bool dump_tacky_file(TackyProgram *p, const char *input_path, DumpTackyFormat fmt, const char *out_path);

#endif

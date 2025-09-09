#ifndef DRIVER_H
#define DRIVER_H

#include <stdbool.h>
#include "../dump/dump.h"

typedef enum {
    DRIVER_STAGE_FULL = 0,   // Run full pipeline
    DRIVER_STAGE_LEX,        // Stop after lexing
    DRIVER_STAGE_PARSE,      // Stop after parsing
    DRIVER_STAGE_TACKY,      // Stop after TACKY generation
    DRIVER_STAGE_CODEGEN     // Stop after code generation (no emission)
} DriverStage;

typedef struct {
    DriverStage stage;
    bool emit_asm;
    const char *input_path;
    
    bool dump_tokens;
    char *dump_tokens_path;
    DumpAstFormat dump_ast_format;
    char *dump_ast_path;
    DumpTackyFormat dump_tacky_format;
    char *dump_tacky_path;
    bool quiet;
} DriverOptions;

DriverOptions driver_parse_args(int argc, char **argv);

void driver_print_usage(const char *prog);

#endif

#include "../../include/driver/driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int has_prefix(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}

void driver_print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [--lex | --parse | --tacky | --codegen] [-S] [--dump-tokens[=<path>]] [--dump-ast[=txt|dot|json] [--dump-ast-path=<path>]] [--dump-tacky[=txt|json] [--dump-tacky-path=<path>]] [--quiet] <source.c>\n"
            "  --lex       Run lexer only (no files written)\n"
            "  --parse     Run lexer+parser (no files written)\n"
            "  --tacky     Run up to TACKY generation (no files written)\n"
            "  --codegen   Run up to assembly generation (no files written)\n"
            "  -S          Emit assembly .s file (no assemble/link)\n"
            "  --dump-tokens[=<path>]  Dump token stream to file (default under out/)\n"
            "  --dump-ast[=fmt]        Dump AST in fmt: txt (default), dot, or json\n"
            "  --dump-ast-path=<path>  Override AST dump path\n"
            "  --dump-tacky[=fmt]      Dump TACKY in fmt: txt (default) or json\n"
            "  --dump-tacky-path=<path> Override TACKY dump path\n"
            "  --quiet                 Suppress stdout prints for AST/assembly\n",
            prog);
}

static char *xstrdup_local(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

DriverOptions driver_parse_args(int argc, char **argv) {
    DriverOptions opts;
    opts.stage = DRIVER_STAGE_FULL;
    opts.emit_asm = false;
    opts.input_path = NULL;
    opts.dump_tokens = false;
    opts.dump_tokens_path = NULL;
    opts.dump_ast_format = DUMP_AST_NONE;
    opts.dump_ast_path = NULL;
    opts.quiet = false;
    opts.dump_tacky_format = DUMP_TACKY_NONE;
    opts.dump_tacky_path = NULL;

    if (argc < 2) {
        driver_print_usage(argv[0]);
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--lex") == 0) {
            if (opts.stage != DRIVER_STAGE_FULL) {
                fprintf(stderr, "Error: Multiple stage flags provided.\n");
                driver_print_usage(argv[0]);
                exit(1);
            }
            opts.stage = DRIVER_STAGE_LEX;
        } else if (strcmp(arg, "--parse") == 0) {
            if (opts.stage != DRIVER_STAGE_FULL) {
                fprintf(stderr, "Error: Multiple stage flags provided.\n");
                driver_print_usage(argv[0]);
                exit(1);
            }
            opts.stage = DRIVER_STAGE_PARSE;
        } else if (strcmp(arg, "--codegen") == 0) {
            if (opts.stage != DRIVER_STAGE_FULL) {
                fprintf(stderr, "Error: Multiple stage flags provided.\n");
                driver_print_usage(argv[0]);
                exit(1);
            }
            opts.stage = DRIVER_STAGE_CODEGEN;
        } else if (strcmp(arg, "--tacky") == 0) {
            if (opts.stage != DRIVER_STAGE_FULL) {
                fprintf(stderr, "Error: Multiple stage flags provided.\n");
                driver_print_usage(argv[0]);
                exit(1);
            }
            opts.stage = DRIVER_STAGE_TACKY;
        } else if (strcmp(arg, "-S") == 0) {
            opts.emit_asm = true;
        } else if (strcmp(arg, "--quiet") == 0) {
            opts.quiet = true;
        } else if (has_prefix(arg, "--dump-tokens")) {
            opts.dump_tokens = true;
            const char *eq = strchr(arg, '=');
            if (eq && *(eq+1)) {
                free(opts.dump_tokens_path);
                opts.dump_tokens_path = xstrdup_local(eq + 1);
            }
        } else if (has_prefix(arg, "--dump-ast-path=")) {
            const char *val = arg + strlen("--dump-ast-path=");
            if (*val) {
                free(opts.dump_ast_path);
                opts.dump_ast_path = xstrdup_local(val);
            }
        } else if (has_prefix(arg, "--dump-ast")) {
            const char *eq = strchr(arg, '=');
            if (!eq) {
                opts.dump_ast_format = DUMP_AST_TXT;
            } else {
                const char *fmt = eq + 1;
                if (strcmp(fmt, "txt") == 0) opts.dump_ast_format = DUMP_AST_TXT;
                else if (strcmp(fmt, "dot") == 0) opts.dump_ast_format = DUMP_AST_DOT;
                else if (strcmp(fmt, "json") == 0) opts.dump_ast_format = DUMP_AST_JSON;
                else {
                    fprintf(stderr, "Unknown AST dump format: %s\n", fmt);
                    driver_print_usage(argv[0]);
                    exit(1);
                }
            }
        } else if (has_prefix(arg, "--dump-tacky-path=")) {
            const char *val = arg + strlen("--dump-tacky-path=");
            if (*val) {
                free(opts.dump_tacky_path);
                opts.dump_tacky_path = xstrdup_local(val);
            }
        } else if (has_prefix(arg, "--dump-tacky")) {
            const char *eq = strchr(arg, '=');
            if (!eq) {
                opts.dump_tacky_format = DUMP_TACKY_TXT; // default
            } else {
                const char *fmt = eq + 1;
                if (strcmp(fmt, "txt") == 0) opts.dump_tacky_format = DUMP_TACKY_TXT;
                else if (strcmp(fmt, "json") == 0) opts.dump_tacky_format = DUMP_TACKY_JSON;
                else {
                    fprintf(stderr, "Unknown TACKY dump format: %s\n", fmt);
                    driver_print_usage(argv[0]);
                    exit(1);
                }
            }
        } else if (has_prefix(arg, "-")) {
            fprintf(stderr, "Unknown option: %s\n", arg);
            driver_print_usage(argv[0]);
            exit(1);
        } else {
            if (opts.input_path != NULL) {
                fprintf(stderr, "Error: Multiple input files provided.\n");
                driver_print_usage(argv[0]);
                exit(1);
            }
            opts.input_path = arg;
        }
    }

    if (opts.input_path == NULL) {
        driver_print_usage(argv[0]);
        exit(1);
    }

    if (opts.stage != DRIVER_STAGE_FULL) {
        opts.emit_asm = false;
    }

    return opts;
}

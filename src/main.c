#include <stdio.h>
#include <stdlib.h>
#include "../include/lexer/lexer.h"
#include "../include/parser/parser.h"
#include "../include/assembly/assembly.h"
#include "../include/assembly/code_emission.h"
#include "../include/driver/driver.h"
#include "../include/tacky/tacky.h"

#ifdef _WIN32
    #include <io.h>
    #define F_OK 0
    #define access _access
#else
    #include <unistd.h>
#endif

char *read_file(const char *filename) {
    if (access(filename, F_OK) != 0) {
        perror("Error: File does not exist");
        return NULL;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(length + 1);
    if (!buffer) {
        perror("Error allocating memory");
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';

    fclose(file);
    return buffer;
}

int main(int argc, char *argv[]) {
    DriverOptions opts = driver_parse_args(argc, argv);

    char *source_code = read_file(opts.input_path);
    if (!source_code) {
        return 1;
    }

    if (opts.stage == DRIVER_STAGE_LEX) {
        if (opts.dump_tokens) {
            if (!dump_tokens_file(opts.input_path, source_code, opts.dump_tokens_path)) {
                fprintf(stderr, "Error: Failed to dump tokens.\n");
                free(source_code);
                return 1;
            }
        } else {
            Lexer lex;
            lexer_init(&lex, source_code);
            for (;;) {
                Token t = lexer_next_token(&lex);
                if (t.type == TOKEN_EOF) { free_token(t); break; }
                free_token(t);
            }
        }
        free(source_code);
        return 0;
    }

    Lexer lexer;
    lexer_init(&lexer, source_code);
    Parser parser;
    parser_init(&parser, &lexer);

    if (opts.stage == DRIVER_STAGE_PARSE) {
        ASTNode *ast = parse_program(&parser);
        if (opts.dump_tokens) {
            if (!dump_tokens_file(opts.input_path, source_code, opts.dump_tokens_path)) {
                fprintf(stderr, "Error: Failed to dump tokens.\n");
                free_ast(ast);
                free(source_code);
                return 1;
            }
        }
        if (opts.dump_ast_format != DUMP_AST_NONE) {
            if (!dump_ast_file(ast, opts.input_path, opts.dump_ast_format, opts.dump_ast_path)) {
                fprintf(stderr, "Error: Failed to dump AST.\n");
                free_ast(ast);
                free(source_code);
                return 1;
            }
        }
        free_ast(ast);
        free(source_code);
        return 0;
    }

    ASTNode *ast = parse_program(&parser);

    if (opts.stage == DRIVER_STAGE_TACKY) {
        TackyProgram *tacky = tacky_from_ast(ast);
        if (opts.dump_tokens) {
            if (!dump_tokens_file(opts.input_path, source_code, opts.dump_tokens_path)) {
                fprintf(stderr, "Error: Failed to dump tokens.\n");
                tacky_free(tacky);
                free_ast(ast);
                free(source_code);
                return 1;
            }
        }
        if (opts.dump_ast_format != DUMP_AST_NONE) {
            if (!dump_ast_file(ast, opts.input_path, opts.dump_ast_format, opts.dump_ast_path)) {
                fprintf(stderr, "Error: Failed to dump AST.\n");
                tacky_free(tacky);
                free_ast(ast);
                free(source_code);
                return 1;
            }
        }
        if (opts.dump_tacky_format != DUMP_TACKY_NONE) {
            if (!dump_tacky_file(tacky, opts.input_path, opts.dump_tacky_format, opts.dump_tacky_path)) {
                fprintf(stderr, "Error: Failed to dump TACKY.\n");
                tacky_free(tacky);
                free_ast(ast);
                free(source_code);
                return 1;
            }
        }
        tacky_free(tacky);
        free_ast(ast);
        free(source_code);
        return 0;
    }

    if (opts.stage == DRIVER_STAGE_CODEGEN) {
        TackyProgram *tacky = tacky_from_ast(ast);
        AssemblyProgram *assembly = generate_assembly(tacky);
        if (opts.dump_tokens) {
            if (!dump_tokens_file(opts.input_path, source_code, opts.dump_tokens_path)) {
                fprintf(stderr, "Error: Failed to dump tokens.\n");
                free_assembly(assembly);
                tacky_free(tacky);
                free_ast(ast);
                free(source_code);
                return 1;
            }
        }
        if (opts.dump_ast_format != DUMP_AST_NONE) {
            if (!dump_ast_file(ast, opts.input_path, opts.dump_ast_format, opts.dump_ast_path)) {
                fprintf(stderr, "Error: Failed to dump AST.\n");
                free_assembly(assembly);
                tacky_free(tacky);
                free_ast(ast);
                free(source_code);
                return 1;
            }
        }
        if (opts.dump_tacky_format != DUMP_TACKY_NONE) {
            (void)dump_tacky_file(tacky, opts.input_path, opts.dump_tacky_format, opts.dump_tacky_path);
        }
        free_assembly(assembly);
        tacky_free(tacky);
        free_ast(ast);
        free(source_code);
        return 0;
    }

    if (!opts.quiet) {
        printf("Abstract Syntax Tree:\n");
        print_ast(ast, 0);
    }

    TackyProgram *tacky = tacky_from_ast(ast);
    AssemblyProgram *assembly = generate_assembly(tacky);
    if (!opts.quiet) {
        print_assembly(assembly);
    }

    if (opts.emit_asm) {
        write_assembly_to_file(assembly, opts.input_path);
    } else {
        int rc = emit_executable_via_cc_pipe(assembly, opts.input_path);
        if (rc == 0 && opts.run_exec) {
            char *bin = get_output_binary_path(opts.input_path);
            (void)run_executable_and_print_exit(bin);
            free(bin);
        }
    }

    if (opts.dump_tokens) {
        (void)dump_tokens_file(opts.input_path, source_code, opts.dump_tokens_path);
    }
    if (opts.dump_ast_format != DUMP_AST_NONE) {
        (void)dump_ast_file(ast, opts.input_path, opts.dump_ast_format, opts.dump_ast_path);
    }
    if (opts.dump_tacky_format != DUMP_TACKY_NONE) {
        (void)dump_tacky_file(tacky, opts.input_path, opts.dump_tacky_format, opts.dump_tacky_path);
    }

    free_ast(ast);
    tacky_free(tacky);
    free_assembly(assembly);
    free(source_code);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include "../include/lexer/lexer.h"
#include "../include/parser/parser.h"
#include "../include/assembly/assembly.h"

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
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <source_file.c>\n", argv[0]);
        return 1
    }

    char *source_code = read_file(argv[1]);
    if (!source_code) {
        return 1;
    }

    Lexer lexer;
    lexer_init(&lexer, source_code);

    Parser parser;
    parser_init(&parser, &lexer);
    ASTNode *ast = parse_program(&parser);

    printf("Abstract Syntax Tree:\n");
    print_ast(ast, 0);

    AssemblyProgram *assembly = generate_assembly(ast);
    print_assembly(assembly);

    free_ast(ast);
    free_assembly(assembly);
    free(source_code);

    return 0;
}

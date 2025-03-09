#include "../../include/assembly/assembly.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <windows.h>

    #define PATH_SEPARATOR '\\'
    #define MKDIR(path) mkdir(path)

    char *strndup(const char *s, size_t n) {
        char *copy = (char *)malloc(n + 1);
        if (!copy) return NULL;
        strncpy(copy, s, n);
        copy[n] = '\0';
        return copy;
    }
#else
    #include <libgen.h> 

    #define MKDIR(path) mkdir(path, 0755)
    #define PATH_SEPARATOR '/'
#endif

static AssemblyInstruction *create_instruction(AssemblyInstructionType type, Operand src, Operand dst) {
    AssemblyInstruction *instr = (AssemblyInstruction *)malloc(sizeof(AssemblyInstruction));
    instr->type = type;
    instr->src = src;
    instr->dst = dst;
    instr->next = NULL;
    return instr;
}

static AssemblyInstruction *generate_instructions(ASTNode *node) {
    if (!node) return NULL;

    switch (node->type) {
        case AST_STATEMENT_RETURN: {
            Operand eax = { .type = OPERAND_REGISTER, .value = 0 }; 

            if (node->left && node->left->type == AST_EXPRESSION_CONSTANT) {
                Operand imm = { .type = OPERAND_IMMEDIATE, .value = atoi(node->left->value) };
                AssemblyInstruction *mov_instr = create_instruction(ASM_MOV, imm, eax);
                AssemblyInstruction *ret_instr = create_instruction(ASM_RET, (Operand){0}, (Operand){0});
                mov_instr->next = ret_instr;
                return mov_instr;
            }
            break;
        }
        default:
            fprintf(stderr, "Unsupported AST Node for Assembly Generation\n");
            exit(1);
    }

    return NULL;
}

AssemblyProgram *generate_assembly(ASTNode *ast) {
    if (!ast || ast->type != AST_PROGRAM || !ast->left) {
        fprintf(stderr, "Invalid AST structure for assembly generation\n");
        exit(1);
    }

    ASTNode *func = ast->left;
    if (func->type != AST_FUNCTION) {
        fprintf(stderr, "Expected a function in AST\n");
        exit(1);
    }

    AssemblyProgram *program = (AssemblyProgram *)malloc(sizeof(AssemblyProgram));
    program->function = (AssemblyFunction *)malloc(sizeof(AssemblyFunction));
    program->function->name = strdup(func->value);
    program->function->instructions = generate_instructions(func->right);

    return program;
}

char *get_output_assembly_path(const char *source_file) {
    char *source_copy = strdup(source_file);
    if (!source_copy) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    char *last_sep = strrchr(source_copy, PATH_SEPARATOR);
    char *directory = last_sep ? strndup(source_copy, last_sep - source_copy + 1) : strdup("./");

    char *filename = strrchr(source_file, PATH_SEPARATOR);
    filename = filename ? filename + 1 : (char *)source_file;

    char *dot = strrchr(filename, '.');
    if (dot && strcmp(dot, ".c") == 0) {
        *dot = '\0';
    }

    size_t path_size = strlen(directory) + strlen(filename) + 4 + 2;
    char *output_path = (char *)malloc(path_size);
    if (!output_path) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(source_copy);
        free(directory);
        exit(1);
    }

    snprintf(output_path, path_size, "%s%c%s.s", directory, PATH_SEPARATOR, filename);

    free(source_copy);
    free(directory);

    return output_path;
}

void write_assembly_to_file(AssemblyProgram *program, const char *source_file) {
    if (!program || !program->function) {
        fprintf(stderr, "Error: No assembly program to write.\n");
        return;
    }

    char *output_path = get_output_assembly_path(source_file);
    FILE *file = fopen(output_path, "w");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s for writing.\n", output_path);
        free(output_path);
        return;
    }

    fprintf(file, ".global main\n");
    fprintf(file, "main:\n");

    AssemblyInstruction *instr = program->function->instructions;
    while (instr) {
        switch (instr->type) {
            case ASM_MOV:
                if (instr->src.type == OPERAND_IMMEDIATE) {
                    fprintf(file, "  movl $%d, %%eax\n", instr->src.value);
                }
                break;
            case ASM_RET:
                fprintf(file, "  ret\n");
                break;
        }
        instr = instr->next;
    }

    fclose(file);
    printf("Assembly written to: %s\n", output_path);
    free(output_path);
}

void print_assembly(AssemblyProgram *program) {
    if (!program || !program->function) return;

    printf("Assembly Code:\n");
    printf("%s:\n", program->function->name);

    AssemblyInstruction *instr = program->function->instructions;
    while (instr) {
        switch (instr->type) {
            case ASM_MOV:
                if (instr->src.type == OPERAND_IMMEDIATE)
                    printf("  mov eax, %d\n", instr->src.value);
                break;
            case ASM_RET:
                printf("  ret\n");
                break;
        }
        instr = instr->next;
    }
}

void free_assembly(AssemblyProgram *program) {
    if (!program) return;

    AssemblyInstruction *instr = program->function->instructions;
    while (instr) {
        AssemblyInstruction *next = instr->next;
        free(instr);
        instr = next;
    }

    free(program->function->name);
    free(program->function);
    free(program);
}

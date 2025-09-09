#include "../../include/assembly/assembly.h"
#include "../../include/tacky/tacky.h"
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

static void append_instr(AssemblyInstruction **head, AssemblyInstruction **tail, AssemblyInstruction *ins) {
    ins->next = NULL;
    if (!*head) { *head = *tail = ins; }
    else { (*tail)->next = ins; *tail = ins; }
}

static AssemblyInstruction *generate_instructions_from_tacky(TackyFunction *fn) {
    if (!fn) return NULL;
    AssemblyInstruction *head = NULL, *tail = NULL;

    char *eax_var = NULL;
    for (TackyInstr *ins = fn->body; ins; ins = ins->next) {
        switch (ins->kind) {
            case TACKY_INSTR_UNARY: {
                if (ins->un_src.kind == TACKY_VAL_CONSTANT) {
                    Operand imm = { .type = OPERAND_IMMEDIATE, .value = ins->un_src.constant };
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    append_instr(&head, &tail, create_instruction(ASM_MOV, imm, eax));
                    eax_var = NULL;
                } else {
                    if (!eax_var || strcmp(eax_var, ins->un_src.var_name) != 0) {
                        // In Chapter 2, sequences are linear; treat as error if violated.
                        fprintf(stderr, "Codegen error: unsupported non-linear TACKY use of %s in unary op.\n", ins->un_src.var_name);
                        exit(1);
                    }
                }

                AssemblyInstructionType op = (ins->un_op == TACKY_UN_NEGATE) ? ASM_NEG : ASM_NOT;
                append_instr(&head, &tail, create_instruction(op, (Operand){ .type = OPERAND_REGISTER, .value = 0 }, (Operand){0}));
                eax_var = ins->un_dst;
                break;
            }
            case TACKY_INSTR_RETURN: {
                if (ins->ret_val.kind == TACKY_VAL_CONSTANT) {
                    Operand imm = { .type = OPERAND_IMMEDIATE, .value = ins->ret_val.constant };
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    append_instr(&head, &tail, create_instruction(ASM_MOV, imm, eax));
                } else {
                    if (!eax_var || strcmp(eax_var, ins->ret_val.var_name) != 0) {
                        fprintf(stderr, "Codegen error: return of non-eax var %s unsupported in Chapter 2.\n", ins->ret_val.var_name);
                        exit(1);
                    }
                }
                append_instr(&head, &tail, create_instruction(ASM_RET, (Operand){0}, (Operand){0}));
                break;
            }
        }
    }

    return head;
}

AssemblyProgram *generate_assembly(TackyProgram *tacky) {
    if (!tacky || !tacky->fn) {
        fprintf(stderr, "Invalid TACKY structure for assembly generation\n");
        exit(1);
    }

    AssemblyProgram *program = (AssemblyProgram *)malloc(sizeof(AssemblyProgram));
    program->function = (AssemblyFunction *)malloc(sizeof(AssemblyFunction));
    program->function->name = strdup(tacky->fn->name);
    program->function->instructions = generate_instructions_from_tacky(tacky->fn);

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
            case ASM_NEG:
                fprintf(file, "  negl %%eax\n");
                break;
            case ASM_NOT:
                fprintf(file, "  notl %%eax\n");
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
                    printf("  movl $%d, %%eax\n", instr->src.value);
                break;
            case ASM_NEG:
                printf("  negl %%eax\n");
                break;
            case ASM_NOT:
                printf("  notl %%eax\n");
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

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

static int is_name_in_list(char **names, int count, const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) return 1;
    }
    return 0;
}

static int collect_temp_vars(TackyFunction *fn, char ***out_names) {
    int cap = 16, count = 0;
    char **names = (char **)malloc(sizeof(char*) * cap);
    for (TackyInstr *ins = fn->body; ins; ins = ins->next) {
        if (ins->kind == TACKY_INSTR_UNARY && ins->un_dst) {
            if (!is_name_in_list(names, count, ins->un_dst)) {
                if (count == cap) { cap *= 2; names = (char**)realloc(names, sizeof(char*) * cap); }
                names[count++] = strdup(ins->un_dst);
            }
        } else if (ins->kind == TACKY_INSTR_BINARY && ins->bin_dst) {
            if (!is_name_in_list(names, count, ins->bin_dst)) {
                if (count == cap) { cap *= 2; names = (char**)realloc(names, sizeof(char*) * cap); }
                names[count++] = strdup(ins->bin_dst);
            }
        }
    }
    *out_names = names;
    return count;
}

static int slot_offset_for(char **names, int n, const char *name) {
    for (int i = 0; i < n; i++) {
        if (strcmp(names[i], name) == 0) {
            // Slots are -4, -8, ... relative to %rbp
            return -4 * (i + 1);
        }
    }
    return 0; // shouldn't happen
}

static const char *reg32_name(int id) {
    switch (id) {
        case 0: return "eax";
        case 1: return "ecx";
        case 2: return "edx";
        case 3: return "ebp";
        default: return "eax";
    }
}

static AssemblyInstruction *generate_instructions_from_tacky(TackyFunction *fn, char **slot_names, int nslots) {
    if (!fn) return NULL;
    AssemblyInstruction *head = NULL, *tail = NULL;
    (void)nslots; // currently unused directly here
    char *eax_var = NULL;
    for (TackyInstr *ins = fn->body; ins; ins = ins->next) {
        switch (ins->kind) {
            case TACKY_INSTR_UNARY: {
                if (ins->un_src.kind == TACKY_VAL_CONSTANT) {
                    Operand imm = { .type = OPERAND_IMMEDIATE, .value = ins->un_src.constant };
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    append_instr(&head, &tail, create_instruction(ASM_MOV, imm, eax));
                } else {
                    // load from slot into %eax
                    int off = slot_offset_for(slot_names, nslots, ins->un_src.var_name);
                    Operand mem = { .type = OPERAND_MEM_RBP_OFFSET, .value = off };
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    append_instr(&head, &tail, create_instruction(ASM_MOV, mem, eax));
                }
                AssemblyInstructionType op = (ins->un_op == TACKY_UN_NEGATE) ? ASM_NEG : ASM_NOT;
                append_instr(&head, &tail, create_instruction(op, (Operand){ .type = OPERAND_REGISTER, .value = 0 }, (Operand){0}));
                if (ins->un_dst) {
                    int dst_off = slot_offset_for(slot_names, nslots, ins->un_dst);
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    Operand memdst = { .type = OPERAND_MEM_RBP_OFFSET, .value = dst_off };
                    append_instr(&head, &tail, create_instruction(ASM_MOV, eax, memdst));
                }
                eax_var = ins->un_dst;
                break;
            }
            case TACKY_INSTR_BINARY: {
                // Load left into %eax, move to %ecx; load right into %eax; perform op
                if (ins->bin_src1.kind == TACKY_VAL_CONSTANT) {
                    Operand imm = { .type = OPERAND_IMMEDIATE, .value = ins->bin_src1.constant };
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    append_instr(&head, &tail, create_instruction(ASM_MOV, imm, eax));
                } else {
                    int off = slot_offset_for(slot_names, nslots, ins->bin_src1.var_name);
                    Operand mem = { .type = OPERAND_MEM_RBP_OFFSET, .value = off };
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    append_instr(&head, &tail, create_instruction(ASM_MOV, mem, eax));
                }
                // movl %eax, %ecx
                append_instr(&head, &tail, create_instruction(ASM_MOV, (Operand){ .type = OPERAND_REGISTER, .value = 0 }, (Operand){ .type = OPERAND_REGISTER, .value = 1 }));

                // Load right into %eax
                if (ins->bin_src2.kind == TACKY_VAL_CONSTANT) {
                    Operand imm = { .type = OPERAND_IMMEDIATE, .value = ins->bin_src2.constant };
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    append_instr(&head, &tail, create_instruction(ASM_MOV, imm, eax));
                } else {
                    int off = slot_offset_for(slot_names, nslots, ins->bin_src2.var_name);
                    Operand mem = { .type = OPERAND_MEM_RBP_OFFSET, .value = off };
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    append_instr(&head, &tail, create_instruction(ASM_MOV, mem, eax));
                }

                switch (ins->bin_op) {
                    case TACKY_BIN_ADD:
                        append_instr(&head, &tail, create_instruction(ASM_ADD_ECX_EAX, (Operand){0}, (Operand){0}));
                        break;
                    case TACKY_BIN_SUB:
                        append_instr(&head, &tail, create_instruction(ASM_SUB_EAX_ECX, (Operand){0}, (Operand){0}));
                        break;
                    case TACKY_BIN_MUL:
                        append_instr(&head, &tail, create_instruction(ASM_IMUL_ECX_EAX, (Operand){0}, (Operand){0}));
                        break;
                    case TACKY_BIN_DIV:
                        // %eax = right, %ecx = left; swap so %eax = left, then cltd; idiv %ecx; quotient in %eax
                        append_instr(&head, &tail, create_instruction(ASM_XCHG_EAX_ECX, (Operand){0}, (Operand){0}));
                        append_instr(&head, &tail, create_instruction(ASM_CLTD, (Operand){0}, (Operand){0}));
                        append_instr(&head, &tail, create_instruction(ASM_IDIV_ECX, (Operand){0}, (Operand){0}));
                        break;
                    case TACKY_BIN_REM:
                        append_instr(&head, &tail, create_instruction(ASM_XCHG_EAX_ECX, (Operand){0}, (Operand){0}));
                        append_instr(&head, &tail, create_instruction(ASM_CLTD, (Operand){0}, (Operand){0}));
                        append_instr(&head, &tail, create_instruction(ASM_IDIV_ECX, (Operand){0}, (Operand){0}));
                        append_instr(&head, &tail, create_instruction(ASM_MOV_EDX_EAX, (Operand){0}, (Operand){0}));
                        break;
                }

                if (ins->bin_dst) {
                    int dst_off = slot_offset_for(slot_names, nslots, ins->bin_dst);
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    Operand memdst = { .type = OPERAND_MEM_RBP_OFFSET, .value = dst_off };
                    append_instr(&head, &tail, create_instruction(ASM_MOV, eax, memdst));
                    eax_var = ins->bin_dst;
                }
                break;
            }
            case TACKY_INSTR_RETURN: {
                if (ins->ret_val.kind == TACKY_VAL_CONSTANT) {
                    Operand imm = { .type = OPERAND_IMMEDIATE, .value = ins->ret_val.constant };
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    append_instr(&head, &tail, create_instruction(ASM_MOV, imm, eax));
                } else {
                    // load from slot into %eax
                    int off = slot_offset_for(slot_names, nslots, ins->ret_val.var_name);
                    Operand mem = { .type = OPERAND_MEM_RBP_OFFSET, .value = off };
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    append_instr(&head, &tail, create_instruction(ASM_MOV, mem, eax));
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

    char **slots = NULL;
    int nslots = collect_temp_vars(tacky->fn, &slots);
    int raw = nslots * 4;
    int aligned = ((raw + 15) / 16) * 16; // 16-byte alignment
    program->function->stack_size = aligned;

    program->function->instructions = generate_instructions_from_tacky(tacky->fn, slots, nslots);

    for (int i = 0; i < nslots; i++) free(slots[i]);
    free(slots);

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

    fprintf(file, ".globl %s\n", program->function->name);
    fprintf(file, "%s:\n", program->function->name);
    fprintf(file, "  pushq %%rbp\n");
    fprintf(file, "  movq %%rsp, %%rbp\n");
    if (program->function->stack_size > 0) {
        fprintf(file, "  subq $%d, %%rsp\n", program->function->stack_size);
    }

    AssemblyInstruction *instr = program->function->instructions;
    while (instr) {
        switch (instr->type) {
            case ASM_MOV:
                if (instr->src.type == OPERAND_IMMEDIATE && instr->dst.type == OPERAND_REGISTER) {
                    fprintf(file, "  movl $%d, %%%s\n", instr->src.value, reg32_name(instr->dst.value));
                } else if (instr->src.type == OPERAND_REGISTER && instr->dst.type == OPERAND_REGISTER) {
                    fprintf(file, "  movl %%%s, %%%s\n", reg32_name(instr->src.value), reg32_name(instr->dst.value));
                } else if (instr->src.type == OPERAND_MEM_RBP_OFFSET && instr->dst.type == OPERAND_REGISTER) {
                    fprintf(file, "  movl %d(%%rbp), %%%s\n", instr->src.value, reg32_name(instr->dst.value));
                } else if (instr->src.type == OPERAND_REGISTER && instr->dst.type == OPERAND_MEM_RBP_OFFSET) {
                    fprintf(file, "  movl %%%s, %d(%%rbp)\n", reg32_name(instr->src.value), instr->dst.value);
                }
                break;
            case ASM_NEG:
                fprintf(file, "  negl %%eax\n");
                break;
            case ASM_NOT:
                fprintf(file, "  notl %%eax\n");
                break;
            case ASM_ADD_ECX_EAX:
                fprintf(file, "  addl %%ecx, %%eax\n");
                break;
            case ASM_SUB_EAX_ECX:
                fprintf(file, "  subl %%eax, %%ecx\n  movl %%ecx, %%eax\n");
                break;
            case ASM_IMUL_ECX_EAX:
                fprintf(file, "  imull %%ecx, %%eax\n");
                break;
            case ASM_XCHG_EAX_ECX:
                fprintf(file, "  xchgl %%eax, %%ecx\n");
                break;
            case ASM_CLTD:
                fprintf(file, "  cltd\n");
                break;
            case ASM_IDIV_ECX:
                fprintf(file, "  idivl %%ecx\n");
                break;
            case ASM_MOV_EDX_EAX:
                fprintf(file, "  movl %%edx, %%eax\n");
                break;
            case ASM_RET:
                fprintf(file, "  leave\n");
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
    printf("  pushq %%rbp\n");
    printf("  movq %%rsp, %%rbp\n");
    if (program->function->stack_size > 0) {
        printf("  subq $%d, %%rsp\n", program->function->stack_size);
    }

    AssemblyInstruction *instr = program->function->instructions;
    while (instr) {
        switch (instr->type) {
            case ASM_MOV:
                if (instr->src.type == OPERAND_IMMEDIATE && instr->dst.type == OPERAND_REGISTER)
                    printf("  movl $%d, %%%s\n", instr->src.value, reg32_name(instr->dst.value));
                else if (instr->src.type == OPERAND_REGISTER && instr->dst.type == OPERAND_REGISTER)
                    printf("  movl %%%s, %%%s\n", reg32_name(instr->src.value), reg32_name(instr->dst.value));
                else if (instr->src.type == OPERAND_MEM_RBP_OFFSET && instr->dst.type == OPERAND_REGISTER)
                    printf("  movl %d(%%rbp), %%%s\n", instr->src.value, reg32_name(instr->dst.value));
                else if (instr->src.type == OPERAND_REGISTER && instr->dst.type == OPERAND_MEM_RBP_OFFSET)
                    printf("  movl %%%s, %d(%%rbp)\n", reg32_name(instr->src.value), instr->dst.value);
                break;
            case ASM_NEG:
                printf("  negl %%eax\n");
                break;
            case ASM_NOT:
                printf("  notl %%eax\n");
                break;
            case ASM_ADD_ECX_EAX:
                printf("  addl %%ecx, %%eax\n");
                break;
            case ASM_SUB_EAX_ECX:
                printf("  subl %%eax, %%ecx\n  movl %%ecx, %%eax\n");
                break;
            case ASM_IMUL_ECX_EAX:
                printf("  imull %%ecx, %%eax\n");
                break;
            case ASM_XCHG_EAX_ECX:
                printf("  xchgl %%eax, %%ecx\n");
                break;
            case ASM_CLTD:
                printf("  cltd\n");
                break;
            case ASM_IDIV_ECX:
                printf("  idivl %%ecx\n");
                break;
            case ASM_MOV_EDX_EAX:
                printf("  movl %%edx, %%eax\n");
                break;
            case ASM_RET:
                printf("  leave\n");
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

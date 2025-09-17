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

// On Mach-O platforms (macOS), C symbols in assembly are prefixed with '_'
#if defined(__APPLE__)
#define GLOBAL_PREFIX "_"
#define LOCAL_LABEL_PREFIX "L"
#else
#define GLOBAL_PREFIX ""
#define LOCAL_LABEL_PREFIX ".L"
#endif

static AssemblyInstruction *create_instruction(AssemblyInstructionType type, Operand src, Operand dst) {
    AssemblyInstruction *instr = (AssemblyInstruction *)malloc(sizeof(AssemblyInstruction));
    instr->type = type;
    instr->src = src;
    instr->dst = dst;
    instr->cond = ASM_COND_NONE;
    instr->label = NULL;
    instr->next = NULL;
    return instr;
}

static char *xstrdup_local(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
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

static void ensure_slot_name(char ***names, int *count, int *cap, const char *name) {
    if (!name) return;
    if (is_name_in_list(*names, *count, name)) return;
    if (*count == *cap) {
        *cap = (*cap == 0) ? 8 : (*cap * 2);
        char **resized = (char **)realloc(*names, sizeof(char *) * (*cap));
        if (!resized) {
            fprintf(stderr, "Out of memory while collecting temporaries\n");
            exit(1);
        }
        *names = resized;
    }
    (*names)[(*count)++] = strdup(name);
}

static void collect_from_val(TackyVal val, char ***names, int *count, int *cap) {
    if (val.kind == TACKY_VAL_VAR && val.var_name) {
        ensure_slot_name(names, count, cap, val.var_name);
    }
}

static int collect_temp_vars(TackyFunction *fn, char ***out_names) {
    int cap = 0, count = 0;
    char **names = NULL;
    for (TackyInstr *ins = fn->body; ins; ins = ins->next) {
        switch (ins->kind) {
            case TACKY_INSTR_UNARY:
                collect_from_val(ins->un_src, &names, &count, &cap);
                ensure_slot_name(&names, &count, &cap, ins->un_dst);
                break;
            case TACKY_INSTR_BINARY:
                collect_from_val(ins->bin_src1, &names, &count, &cap);
                collect_from_val(ins->bin_src2, &names, &count, &cap);
                ensure_slot_name(&names, &count, &cap, ins->bin_dst);
                break;
            case TACKY_INSTR_COPY:
                collect_from_val(ins->copy_src, &names, &count, &cap);
                ensure_slot_name(&names, &count, &cap, ins->copy_dst);
                break;
            case TACKY_INSTR_JUMP_IF_ZERO:
            case TACKY_INSTR_JUMP_IF_NOT_ZERO:
                collect_from_val(ins->cond_val, &names, &count, &cap);
                break;
            case TACKY_INSTR_RETURN:
                collect_from_val(ins->ret_val, &names, &count, &cap);
                break;
            default:
                break;
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
        case 4: return "r10d";
        case 5: return "r11d";
        default: return "eax";
    }
}

static const char *reg8_name(int id) {
    switch (id) {
        case 0: return "al";
        case 1: return "cl";
        case 2: return "dl";
        case 4: return "r10b";
        case 5: return "r11b";
        default: return "al";
    }
}

static void print_operand(FILE *out, Operand op, int byte_reg) {
    switch (op.type) {
        case OPERAND_IMMEDIATE:
            fprintf(out, "$%d", op.value);
            break;
        case OPERAND_REGISTER:
            fprintf(out, "%%%s", byte_reg ? reg8_name(op.value) : reg32_name(op.value));
            break;
        case OPERAND_MEM_RBP_OFFSET:
            fprintf(out, "%d(%%rbp)", op.value);
            break;
    }
}

static Operand operand_from_val(TackyVal val, char **slot_names, int nslots) {
    if (val.kind == TACKY_VAL_CONSTANT) {
        Operand op = { .type = OPERAND_IMMEDIATE, .value = val.constant };
        return op;
    }
    int off = slot_offset_for(slot_names, nslots, val.var_name);
    Operand op = { .type = OPERAND_MEM_RBP_OFFSET, .value = off };
    return op;
}

static void append_cmp_with_fixups(AssemblyInstruction **head, AssemblyInstruction **tail, Operand left, Operand right) {
    // Ensure the second operand is not an immediate and avoid mem/mem combos.
    if (right.type == OPERAND_IMMEDIATE) {
        Operand reg = { .type = OPERAND_REGISTER, .value = 5 }; // r11d
        append_instr(head, tail, create_instruction(ASM_MOV, right, reg));
        right = reg;
    }
    if (right.type == OPERAND_MEM_RBP_OFFSET && left.type == OPERAND_MEM_RBP_OFFSET) {
        Operand reg = { .type = OPERAND_REGISTER, .value = 4 }; // r10d
        append_instr(head, tail, create_instruction(ASM_MOV, right, reg));
        right = reg;
    }

    AssemblyInstruction *cmp = create_instruction(ASM_CMP, left, right);
    append_instr(head, tail, cmp);
}

static void append_move_with_fixups(AssemblyInstruction **head, AssemblyInstruction **tail, Operand src, Operand dst) {
    if (src.type == OPERAND_MEM_RBP_OFFSET && dst.type == OPERAND_MEM_RBP_OFFSET) {
        Operand reg = { .type = OPERAND_REGISTER, .value = 5 }; // r11d scratch
        append_instr(head, tail, create_instruction(ASM_MOV, src, reg));
        append_instr(head, tail, create_instruction(ASM_MOV, reg, dst));
        return;
    }
    append_instr(head, tail, create_instruction(ASM_MOV, src, dst));
}

static AssemblyCondCode cond_from_relop(TackyBinaryOp op) {
    switch (op) {
        case TACKY_BIN_EQUAL: return ASM_COND_E;
        case TACKY_BIN_NOT_EQUAL: return ASM_COND_NE;
        case TACKY_BIN_LESS: return ASM_COND_L;
        case TACKY_BIN_LESS_EQUAL: return ASM_COND_LE;
        case TACKY_BIN_GREATER: return ASM_COND_G;
        case TACKY_BIN_GREATER_EQUAL: return ASM_COND_GE;
        default: return ASM_COND_NONE;
    }
}

static int is_relational_binop(TackyBinaryOp op) {
    switch (op) {
        case TACKY_BIN_EQUAL:
        case TACKY_BIN_NOT_EQUAL:
        case TACKY_BIN_LESS:
        case TACKY_BIN_LESS_EQUAL:
        case TACKY_BIN_GREATER:
        case TACKY_BIN_GREATER_EQUAL:
            return 1;
        default:
            return 0;
    }
}

static const char *cond_suffix(AssemblyCondCode cond) {
    switch (cond) {
        case ASM_COND_E: return "e";
        case ASM_COND_NE: return "ne";
        case ASM_COND_L: return "l";
        case ASM_COND_LE: return "le";
        case ASM_COND_G: return "g";
        case ASM_COND_GE: return "ge";
        default: return "";
    }
}

static AssemblyInstruction *generate_instructions_from_tacky(TackyFunction *fn, char **slot_names, int nslots) {
    if (!fn) return NULL;
    AssemblyInstruction *head = NULL, *tail = NULL;

    for (TackyInstr *ins = fn->body; ins; ins = ins->next) {
        switch (ins->kind) {
            case TACKY_INSTR_UNARY: {
                if (ins->un_op == TACKY_UN_NOT) {
                    Operand zero = { .type = OPERAND_IMMEDIATE, .value = 0 };
                    Operand cond_op = operand_from_val(ins->un_src, slot_names, nslots);
                    append_cmp_with_fixups(&head, &tail, zero, cond_op);
                    if (ins->un_dst) {
                        Operand dst = { .type = OPERAND_MEM_RBP_OFFSET, .value = slot_offset_for(slot_names, nslots, ins->un_dst) };
                        append_move_with_fixups(&head, &tail, zero, dst);
                        AssemblyInstruction *set = create_instruction(ASM_SETCC, (Operand){0}, dst);
                        set->cond = ASM_COND_E;
                        append_instr(&head, &tail, set);
                    }
                } else {
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    Operand dst = {0};
                    if (ins->un_dst) {
                        dst.type = OPERAND_MEM_RBP_OFFSET;
                        dst.value = slot_offset_for(slot_names, nslots, ins->un_dst);
                    }
                    Operand src = operand_from_val(ins->un_src, slot_names, nslots);
                    append_move_with_fixups(&head, &tail, src, eax);
                    AssemblyInstructionType op = (ins->un_op == TACKY_UN_NEGATE) ? ASM_NEG : ASM_NOT;
                    append_instr(&head, &tail, create_instruction(op, eax, (Operand){0}));
                    if (ins->un_dst) {
                        append_move_with_fixups(&head, &tail, eax, dst);
                    }
                }
                break;
            }
            case TACKY_INSTR_BINARY: {
                if (is_relational_binop(ins->bin_op)) {
                    Operand left = operand_from_val(ins->bin_src2, slot_names, nslots);
                    Operand right = operand_from_val(ins->bin_src1, slot_names, nslots);
                    append_cmp_with_fixups(&head, &tail, left, right);
                    if (ins->bin_dst) {
                        Operand dst = { .type = OPERAND_MEM_RBP_OFFSET, .value = slot_offset_for(slot_names, nslots, ins->bin_dst) };
                        Operand zero = { .type = OPERAND_IMMEDIATE, .value = 0 };
                        append_move_with_fixups(&head, &tail, zero, dst);
                        AssemblyInstruction *set = create_instruction(ASM_SETCC, (Operand){0}, dst);
                        set->cond = cond_from_relop(ins->bin_op);
                        append_instr(&head, &tail, set);
                    }
                } else {
                    Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                    Operand ecx = { .type = OPERAND_REGISTER, .value = 1 };
                    Operand src1 = operand_from_val(ins->bin_src1, slot_names, nslots);
                    Operand src2 = operand_from_val(ins->bin_src2, slot_names, nslots);
                    append_move_with_fixups(&head, &tail, src1, eax);
                    append_move_with_fixups(&head, &tail, eax, ecx);
                    append_move_with_fixups(&head, &tail, src2, eax);

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
                        default:
                            break;
                    }

                    if (ins->bin_dst) {
                        Operand dst = { .type = OPERAND_MEM_RBP_OFFSET, .value = slot_offset_for(slot_names, nslots, ins->bin_dst) };
                        append_move_with_fixups(&head, &tail, eax, dst);
                    }
                }
                break;
            }
            case TACKY_INSTR_COPY: {
                Operand src = operand_from_val(ins->copy_src, slot_names, nslots);
                Operand dst = { .type = OPERAND_MEM_RBP_OFFSET, .value = slot_offset_for(slot_names, nslots, ins->copy_dst) };
                append_move_with_fixups(&head, &tail, src, dst);
                break;
            }
            case TACKY_INSTR_JUMP: {
                AssemblyInstruction *jmp = create_instruction(ASM_JMP, (Operand){0}, (Operand){0});
                jmp->label = xstrdup_local(ins->jump_target);
                append_instr(&head, &tail, jmp);
                break;
            }
            case TACKY_INSTR_JUMP_IF_ZERO: {
                Operand zero = { .type = OPERAND_IMMEDIATE, .value = 0 };
                Operand cond = operand_from_val(ins->cond_val, slot_names, nslots);
                append_cmp_with_fixups(&head, &tail, zero, cond);
                AssemblyInstruction *jcc = create_instruction(ASM_JCC, (Operand){0}, (Operand){0});
                jcc->cond = ASM_COND_E;
                jcc->label = xstrdup_local(ins->jump_target);
                append_instr(&head, &tail, jcc);
                break;
            }
            case TACKY_INSTR_JUMP_IF_NOT_ZERO: {
                Operand zero = { .type = OPERAND_IMMEDIATE, .value = 0 };
                Operand cond = operand_from_val(ins->cond_val, slot_names, nslots);
                append_cmp_with_fixups(&head, &tail, zero, cond);
                AssemblyInstruction *jcc = create_instruction(ASM_JCC, (Operand){0}, (Operand){0});
                jcc->cond = ASM_COND_NE;
                jcc->label = xstrdup_local(ins->jump_target);
                append_instr(&head, &tail, jcc);
                break;
            }
            case TACKY_INSTR_LABEL: {
                AssemblyInstruction *lab = create_instruction(ASM_LABEL, (Operand){0}, (Operand){0});
                lab->label = xstrdup_local(ins->label);
                append_instr(&head, &tail, lab);
                break;
            }
            case TACKY_INSTR_RETURN: {
                Operand eax = { .type = OPERAND_REGISTER, .value = 0 };
                Operand src = operand_from_val(ins->ret_val, slot_names, nslots);
                append_move_with_fixups(&head, &tail, src, eax);
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

    write_assembly_to_stream(program, file);

    fclose(file);
    printf("Assembly written to: %s\n", output_path);
    free(output_path);
}

void print_assembly(AssemblyProgram *program) {
    if (!program || !program->function) return;

    printf("Assembly Code:\n");
    write_assembly_to_stream(program, stdout);
}

void free_assembly(AssemblyProgram *program) {
    if (!program) return;

    AssemblyInstruction *instr = program->function->instructions;
    while (instr) {
        AssemblyInstruction *next = instr->next;
        free(instr->label);
        free(instr);
        instr = next;
    }

    free(program->function->name);
    free(program->function);
    free(program);
}

void write_assembly_to_stream(AssemblyProgram *program, FILE *out) {
    if (!program || !program->function || !out) return;

    fprintf(out, ".globl %s%s\n", GLOBAL_PREFIX, program->function->name);
    fprintf(out, "%s%s:\n", GLOBAL_PREFIX, program->function->name);
    fprintf(out, "  pushq %%rbp\n");
    fprintf(out, "  movq %%rsp, %%rbp\n");
    if (program->function->stack_size > 0) {
        fprintf(out, "  subq $%d, %%rsp\n", program->function->stack_size);
    }

    AssemblyInstruction *instr = program->function->instructions;
    while (instr) {
        switch (instr->type) {
            case ASM_MOV:
                fprintf(out, "  movl ");
                print_operand(out, instr->src, 0);
                fprintf(out, ", ");
                print_operand(out, instr->dst, 0);
                fprintf(out, "\n");
                break;
            case ASM_NEG:
                fprintf(out, "  negl %%eax\n");
                break;
            case ASM_NOT:
                fprintf(out, "  notl %%eax\n");
                break;
            case ASM_ADD_ECX_EAX:
                fprintf(out, "  addl %%ecx, %%eax\n");
                break;
            case ASM_SUB_EAX_ECX:
                fprintf(out, "  subl %%eax, %%ecx\n  movl %%ecx, %%eax\n");
                break;
            case ASM_IMUL_ECX_EAX:
                fprintf(out, "  imull %%ecx, %%eax\n");
                break;
            case ASM_XCHG_EAX_ECX:
                fprintf(out, "  xchgl %%eax, %%ecx\n");
                break;
            case ASM_CLTD:
                fprintf(out, "  cltd\n");
                break;
            case ASM_IDIV_ECX:
                fprintf(out, "  idivl %%ecx\n");
                break;
            case ASM_MOV_EDX_EAX:
                fprintf(out, "  movl %%edx, %%eax\n");
                break;
            case ASM_CMP:
                fprintf(out, "  cmpl ");
                print_operand(out, instr->src, 0);
                fprintf(out, ", ");
                print_operand(out, instr->dst, 0);
                fprintf(out, "\n");
                break;
            case ASM_SETCC:
                fprintf(out, "  set%s ", cond_suffix(instr->cond));
                print_operand(out, instr->dst, 1);
                fprintf(out, "\n");
                break;
            case ASM_JMP:
                fprintf(out, "  jmp %s%s\n", LOCAL_LABEL_PREFIX, instr->label);
                break;
            case ASM_JCC:
                fprintf(out, "  j%s %s%s\n", cond_suffix(instr->cond), LOCAL_LABEL_PREFIX, instr->label);
                break;
            case ASM_LABEL:
                fprintf(out, "%s%s:\n", LOCAL_LABEL_PREFIX, instr->label);
                break;
            case ASM_RET:
                fprintf(out, "  leave\n");
                fprintf(out, "  ret\n");
                break;
        }
        instr = instr->next;
    }
}

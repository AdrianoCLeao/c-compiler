#include "../../include/assembly/assembly.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

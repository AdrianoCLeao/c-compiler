#ifndef ASSEMBLY_H
#define ASSEMBLY_H

#include "../tacky/tacky.h"

typedef enum {
    ASM_MOV,
    ASM_RET,
    ASM_NEG,
    ASM_NOT,
    ASM_ADD_ECX_EAX,
    ASM_SUB_EAX_ECX,
    ASM_IMUL_ECX_EAX,
    ASM_IDIV_ECX,
    ASM_MOV_EDX_EAX,
    ASM_XCHG_EAX_ECX,
    ASM_CLTD,
} AssemblyInstructionType;

typedef enum {
    OPERAND_IMMEDIATE,
    OPERAND_REGISTER,
    OPERAND_MEM_RBP_OFFSET
} OperandType;

typedef struct {
    OperandType type;
    int value;  
} Operand;

typedef struct AssemblyInstruction {
    AssemblyInstructionType type;
    Operand src;
    Operand dst;
    struct AssemblyInstruction *next;
} AssemblyInstruction;

typedef struct {
    char *name;
    AssemblyInstruction *instructions;
    int stack_size;
} AssemblyFunction;

typedef struct {
    AssemblyFunction *function;
} AssemblyProgram;

AssemblyProgram *generate_assembly(TackyProgram *tacky);
void print_assembly(AssemblyProgram *program);
void free_assembly(AssemblyProgram *program);
char *get_output_assembly_path(const char *source_file);
void write_assembly_to_file(AssemblyProgram *program, const char *source_file);

#endif 

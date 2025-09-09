#ifndef ASSEMBLY_H
#define ASSEMBLY_H

#include "../tacky/tacky.h"

typedef enum {
    ASM_MOV,
    ASM_RET,
    ASM_NEG,
    ASM_NOT,
} AssemblyInstructionType;

typedef enum {
    OPERAND_IMMEDIATE,
    OPERAND_REGISTER
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

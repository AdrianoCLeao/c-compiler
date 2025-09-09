#ifndef TACKY_H
#define TACKY_H

#include "../parser/parser.h"

typedef enum {
    TACKY_VAL_CONSTANT,
    TACKY_VAL_VAR
} TackyValKind;

typedef struct {
    TackyValKind kind;
    int constant;      // valid if kind == TACKY_VAL_CONSTANT
    char *var_name;    // valid if kind == TACKY_VAL_VAR
} TackyVal;

typedef enum {
    TACKY_UN_NEGATE,
    TACKY_UN_COMPLEMENT
} TackyUnaryOp;

typedef enum {
    TACKY_INSTR_RETURN,
    TACKY_INSTR_UNARY
} TackyInstrKind;

typedef struct TackyInstr {
    TackyInstrKind kind;
    TackyVal ret_val;
    TackyUnaryOp un_op;
    TackyVal un_src;
    char *un_dst; // destination variable name

    struct TackyInstr *next;
} TackyInstr;

typedef struct {
    char *name;         // function name
    TackyInstr *body;   // linked list of instructions
} TackyFunction;

typedef struct {
    TackyFunction *fn;
} TackyProgram;

TackyProgram *tacky_from_ast(ASTNode *ast);

void tacky_print_txt(TackyProgram *p);
void tacky_print_json(TackyProgram *p);

void tacky_free(TackyProgram *p);

#endif


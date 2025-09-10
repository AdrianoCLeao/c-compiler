#include "../../include/tacky/tacky.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    int temp_counter;
    TackyInstr *head;
    TackyInstr *tail;
} TackyGenCtx;

static char *xstrdup_local(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

static char *make_temp(TackyGenCtx *ctx) {
    char buf[32];
    snprintf(buf, sizeof(buf), "t%d", ctx->temp_counter++);
    return xstrdup_local(buf);
}

static void emit_instr(TackyGenCtx *ctx, TackyInstr *ins) {
    ins->next = NULL;
    if (!ctx->head) {
        ctx->head = ctx->tail = ins;
    } else {
        ctx->tail->next = ins;
        ctx->tail = ins;
    }
}

static TackyVal tv_const(int v) {
    TackyVal t; t.kind = TACKY_VAL_CONSTANT; t.constant = v; t.var_name = NULL; return t;
}
static TackyVal tv_var(const char *name) {
    TackyVal t; t.kind = TACKY_VAL_VAR; t.constant = 0; t.var_name = (char *)name; return t;
}

static TackyUnaryOp convert_unop(ASTNodeType t) {
    switch (t) {
        case AST_EXPRESSION_NEGATE: return TACKY_UN_NEGATE;
        case AST_EXPRESSION_COMPLEMENT: return TACKY_UN_COMPLEMENT;
        default: return TACKY_UN_NEGATE; // unreachable for Chapter 2
    }
}

static TackyBinaryOp convert_binop(ASTNodeType t) {
    switch (t) {
        case AST_EXPRESSION_ADD: return TACKY_BIN_ADD;
        case AST_EXPRESSION_SUBTRACT: return TACKY_BIN_SUB;
        case AST_EXPRESSION_MULTIPLY: return TACKY_BIN_MUL;
        case AST_EXPRESSION_DIVIDE: return TACKY_BIN_DIV;
        case AST_EXPRESSION_REMAINDER: return TACKY_BIN_REM;
        default: return TACKY_BIN_ADD; // unreachable
    }
}

static TackyVal gen_exp(ASTNode *e, TackyGenCtx *ctx) {
    switch (e->type) {
        case AST_EXPRESSION_CONSTANT: {
            int v = atoi(e->value);
            return tv_const(v);
        }
        case AST_EXPRESSION_NEGATE:
        case AST_EXPRESSION_COMPLEMENT: {
            TackyVal src = gen_exp(e->left, ctx);
            char *dst = make_temp(ctx);
            TackyInstr *ins = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            ins->kind = TACKY_INSTR_UNARY;
            ins->un_op = convert_unop(e->type);
            ins->un_src = src;
            ins->un_dst = dst;
            emit_instr(ctx, ins);
            return tv_var(dst);
        }
        case AST_EXPRESSION_ADD:
        case AST_EXPRESSION_SUBTRACT:
        case AST_EXPRESSION_MULTIPLY:
        case AST_EXPRESSION_DIVIDE:
        case AST_EXPRESSION_REMAINDER: {
            TackyVal v1 = gen_exp(e->left, ctx);
            TackyVal v2 = gen_exp(e->right, ctx);
            char *dst = make_temp(ctx);
            TackyInstr *ins = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            ins->kind = TACKY_INSTR_BINARY;
            ins->bin_op = convert_binop(e->type);
            ins->bin_src1 = v1;
            ins->bin_src2 = v2;
            ins->bin_dst = dst;
            emit_instr(ctx, ins);
            return tv_var(dst);
        }
        default:
            return tv_const(0);
    }
}

TackyProgram *tacky_from_ast(ASTNode *ast) {
    if (!ast || ast->type != AST_PROGRAM || !ast->left) return NULL;
    ASTNode *fn = ast->left;
    if (!fn || fn->type != AST_FUNCTION) return NULL;

    TackyGenCtx ctx = {0};
    ASTNode *stmt = fn->right;
    TackyVal retv = tv_const(0);
    if (stmt && stmt->type == AST_STATEMENT_RETURN) {
        retv = gen_exp(stmt->left, &ctx);
    }

    TackyInstr *retins = (TackyInstr *)calloc(1, sizeof(TackyInstr));
    retins->kind = TACKY_INSTR_RETURN;
    retins->ret_val = retv;
    emit_instr(&ctx, retins);

    TackyProgram *p = (TackyProgram *)malloc(sizeof(TackyProgram));
    p->fn = (TackyFunction *)malloc(sizeof(TackyFunction));
    p->fn->name = fn->value ? xstrdup_local(fn->value) : xstrdup_local("main");
    p->fn->body = ctx.head;
    return p;
}

static const char *unop_name(TackyUnaryOp op) {
    switch (op) {
        case TACKY_UN_NEGATE: return "Negate";
        case TACKY_UN_COMPLEMENT: return "Complement";
        default: return "?";
    }
}

void tacky_print_txt(TackyProgram *p) {
    if (!p || !p->fn) return;
    printf("Function %s()\n", p->fn->name);
    for (TackyInstr *ins = p->fn->body; ins; ins = ins->next) {
        switch (ins->kind) {
            case TACKY_INSTR_UNARY:
                if (ins->un_src.kind == TACKY_VAL_CONSTANT)
                    printf("  %s %d -> %s\n", unop_name(ins->un_op), ins->un_src.constant, ins->un_dst);
                else
                    printf("  %s %s -> %s\n", unop_name(ins->un_op), ins->un_src.var_name, ins->un_dst);
                break;
            case TACKY_INSTR_BINARY: {
                const char *op = "?";
                switch (ins->bin_op) {
                    case TACKY_BIN_ADD: op = "Add"; break;
                    case TACKY_BIN_SUB: op = "Subtract"; break;
                    case TACKY_BIN_MUL: op = "Multiply"; break;
                    case TACKY_BIN_DIV: op = "Divide"; break;
                    case TACKY_BIN_REM: op = "Remainder"; break;
                }
                printf("  %s ", op);
                if (ins->bin_src1.kind == TACKY_VAL_CONSTANT) printf("%d, ", ins->bin_src1.constant);
                else printf("%s, ", ins->bin_src1.var_name);
                if (ins->bin_src2.kind == TACKY_VAL_CONSTANT) printf("%d ", ins->bin_src2.constant);
                else printf("%s ", ins->bin_src2.var_name);
                printf("-> %s\n", ins->bin_dst);
                break;
            }
            case TACKY_INSTR_RETURN:
                if (ins->ret_val.kind == TACKY_VAL_CONSTANT)
                    printf("  Return %d\n", ins->ret_val.constant);
                else
                    printf("  Return %s\n", ins->ret_val.var_name);
                break;
        }
    }
}

static void json_escape(FILE *f, const char *s) {
    for (const char *p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"': fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if (c < 0x20) fprintf(f, "\\u%04x", c);
                else fputc(c, f);
        }
    }
}

void tacky_print_json(TackyProgram *p) {
    if (!p || !p->fn) { printf("null\n"); return; }
    printf("{\n  \"function\": \"%s\",\n  \"body\": [\n", p->fn->name);
    int first = 1;
    for (TackyInstr *ins = p->fn->body; ins; ins = ins->next) {
        if (!first) printf(",\n");
        first = 0;
        printf("    {");
        if (ins->kind == TACKY_INSTR_UNARY) {
            printf("\"kind\": \"Unary\", ");
            printf("\"op\": \"%s\", ", unop_name(ins->un_op));
            printf("\"src\": ");
            if (ins->un_src.kind == TACKY_VAL_CONSTANT) printf("{\"const\": %d}", ins->un_src.constant);
            else { printf("{\"var\": \""); json_escape(stdout, ins->un_src.var_name); printf("\"}"); }
            printf(", \"dst\": \""); json_escape(stdout, ins->un_dst); printf("\"");
        } else if (ins->kind == TACKY_INSTR_BINARY) {
            const char *op = "?";
            switch (ins->bin_op) {
                case TACKY_BIN_ADD: op = "Add"; break;
                case TACKY_BIN_SUB: op = "Subtract"; break;
                case TACKY_BIN_MUL: op = "Multiply"; break;
                case TACKY_BIN_DIV: op = "Divide"; break;
                case TACKY_BIN_REM: op = "Remainder"; break;
            }
            printf("\"kind\": \"Binary\", ");
            printf("\"op\": \"%s\", ", op);
            printf("\"src1\": ");
            if (ins->bin_src1.kind == TACKY_VAL_CONSTANT) printf("{\"const\": %d}", ins->bin_src1.constant);
            else { printf("{\"var\": \""); json_escape(stdout, ins->bin_src1.var_name); printf("\"}"); }
            printf(", \"src2\": ");
            if (ins->bin_src2.kind == TACKY_VAL_CONSTANT) printf("{\"const\": %d}", ins->bin_src2.constant);
            else { printf("{\"var\": \""); json_escape(stdout, ins->bin_src2.var_name); printf("\"}"); }
            printf(", \"dst\": \""); json_escape(stdout, ins->bin_dst); printf("\"");
        } else if (ins->kind == TACKY_INSTR_RETURN) {
            printf("\"kind\": \"Return\", \"value\": ");
            if (ins->ret_val.kind == TACKY_VAL_CONSTANT) printf("{\"const\": %d}", ins->ret_val.constant);
            else { printf("{\"var\": \""); json_escape(stdout, ins->ret_val.var_name); printf("\"}"); }
        }
        printf("}");
    }
    printf("\n  ]\n}\n");
}

void tacky_free(TackyProgram *p) {
    if (!p) return;
    if (p->fn) {
        TackyInstr *ins = p->fn->body;
        while (ins) {
            TackyInstr *n = ins->next;
            if (ins->kind == TACKY_INSTR_UNARY) {
                free(ins->un_dst);
            } else if (ins->kind == TACKY_INSTR_BINARY) {
                free(ins->bin_dst);
            }
            free(ins);
            ins = n;
        }
        free(p->fn->name);
        free(p->fn);
    }
    free(p);
}

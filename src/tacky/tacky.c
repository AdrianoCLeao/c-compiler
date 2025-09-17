#include "../../include/tacky/tacky.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    int temp_counter;
    int label_counter;
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

static char *make_label(TackyGenCtx *ctx, const char *prefix) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s%d", prefix, ctx->label_counter++);
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
    TackyVal t; t.kind = TACKY_VAL_CONSTANT; t.constant = v; t.var_name = NULL; t.owns_name = false; return t;
}
static TackyVal tv_var(const char *name, bool take_ownership) {
    TackyVal t; t.kind = TACKY_VAL_VAR; t.constant = 0;
    if (take_ownership) {
        t.var_name = xstrdup_local(name);
        if (!t.var_name) {
            fprintf(stderr, "Out of memory while duplicating variable name\n");
            exit(1);
        }
        t.owns_name = true;
    } else {
        t.var_name = (char *)name;
        t.owns_name = false;
    }
    return t;
}

static TackyUnaryOp convert_unop(ASTNodeType t) {
    switch (t) {
        case AST_EXPRESSION_NEGATE: return TACKY_UN_NEGATE;
        case AST_EXPRESSION_COMPLEMENT: return TACKY_UN_COMPLEMENT;
        case AST_EXPRESSION_NOT: return TACKY_UN_NOT;
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
        case AST_EXPRESSION_EQUAL: return TACKY_BIN_EQUAL;
        case AST_EXPRESSION_NOT_EQUAL: return TACKY_BIN_NOT_EQUAL;
        case AST_EXPRESSION_LESS_THAN: return TACKY_BIN_LESS;
        case AST_EXPRESSION_LESS_EQUAL: return TACKY_BIN_LESS_EQUAL;
        case AST_EXPRESSION_GREATER_THAN: return TACKY_BIN_GREATER;
        case AST_EXPRESSION_GREATER_EQUAL: return TACKY_BIN_GREATER_EQUAL;
        default: return TACKY_BIN_ADD; // unreachable
    }
}

static TackyVal gen_exp(ASTNode *e, TackyGenCtx *ctx) {
    switch (e->type) {
        case AST_EXPRESSION_CONSTANT: {
            int v = atoi(e->value);
            return tv_const(v);
        }
        case AST_EXPRESSION_VARIABLE:
            return tv_var(e->value, true);
        case AST_EXPRESSION_ASSIGNMENT: {
            if (!e->left || e->left->type != AST_EXPRESSION_VARIABLE) {
                return tv_const(0);
            }
            const char *name = e->left->value;
            TackyVal rhs = gen_exp(e->right, ctx);
            TackyInstr *copy = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            copy->kind = TACKY_INSTR_COPY;
            copy->copy_src = rhs;
            copy->copy_dst = xstrdup_local(name);
            emit_instr(ctx, copy);
            return tv_var(name, true);
        }
        case AST_EXPRESSION_NEGATE:
        case AST_EXPRESSION_COMPLEMENT:
        case AST_EXPRESSION_NOT: {
            TackyVal src = gen_exp(e->left, ctx);
            char *dst = make_temp(ctx);
            TackyInstr *ins = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            ins->kind = TACKY_INSTR_UNARY;
            ins->un_op = convert_unop(e->type);
            ins->un_src = src;
            ins->un_dst = dst;
            emit_instr(ctx, ins);
            return tv_var(dst, false);
        }
        case AST_EXPRESSION_ADD:
        case AST_EXPRESSION_SUBTRACT:
        case AST_EXPRESSION_MULTIPLY:
        case AST_EXPRESSION_DIVIDE:
        case AST_EXPRESSION_REMAINDER:
        case AST_EXPRESSION_EQUAL:
        case AST_EXPRESSION_NOT_EQUAL:
        case AST_EXPRESSION_LESS_THAN:
        case AST_EXPRESSION_LESS_EQUAL:
        case AST_EXPRESSION_GREATER_THAN:
        case AST_EXPRESSION_GREATER_EQUAL: {
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
            return tv_var(dst, false);
        }
        case AST_EXPRESSION_LOGICAL_AND: {
            TackyVal left = gen_exp(e->left, ctx);
            char *result = make_temp(ctx);
            char *false_label = make_label(ctx, "and_false");
            char *end_label = make_label(ctx, "and_end");

            TackyInstr *jump1 = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            jump1->kind = TACKY_INSTR_JUMP_IF_ZERO;
            jump1->cond_val = left;
            jump1->jump_target = xstrdup_local(false_label);
            emit_instr(ctx, jump1);

            TackyVal right = gen_exp(e->right, ctx);
            TackyInstr *jump2 = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            jump2->kind = TACKY_INSTR_JUMP_IF_ZERO;
            jump2->cond_val = right;
            jump2->jump_target = xstrdup_local(false_label);
            emit_instr(ctx, jump2);

            TackyInstr *copy_true = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            copy_true->kind = TACKY_INSTR_COPY;
            copy_true->copy_src = tv_const(1);
            copy_true->copy_dst = result;
            emit_instr(ctx, copy_true);

            TackyInstr *jump_end = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            jump_end->kind = TACKY_INSTR_JUMP;
            jump_end->jump_target = xstrdup_local(end_label);
            emit_instr(ctx, jump_end);

            TackyInstr *label_false = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            label_false->kind = TACKY_INSTR_LABEL;
            label_false->label = false_label;
            emit_instr(ctx, label_false);

            TackyInstr *copy_false = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            copy_false->kind = TACKY_INSTR_COPY;
            copy_false->copy_src = tv_const(0);
            copy_false->copy_dst = xstrdup_local(result);
            emit_instr(ctx, copy_false);

            TackyInstr *label_end = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            label_end->kind = TACKY_INSTR_LABEL;
            label_end->label = end_label;
            emit_instr(ctx, label_end);

            return tv_var(result, false);
        }
        case AST_EXPRESSION_LOGICAL_OR: {
            TackyVal left = gen_exp(e->left, ctx);
            char *result = make_temp(ctx);
            char *true_label = make_label(ctx, "or_true");
            char *end_label = make_label(ctx, "or_end");

            TackyInstr *jump1 = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            jump1->kind = TACKY_INSTR_JUMP_IF_NOT_ZERO;
            jump1->cond_val = left;
            jump1->jump_target = xstrdup_local(true_label);
            emit_instr(ctx, jump1);

            TackyVal right = gen_exp(e->right, ctx);
            TackyInstr *jump2 = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            jump2->kind = TACKY_INSTR_JUMP_IF_NOT_ZERO;
            jump2->cond_val = right;
            jump2->jump_target = xstrdup_local(true_label);
            emit_instr(ctx, jump2);

            TackyInstr *copy_false = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            copy_false->kind = TACKY_INSTR_COPY;
            copy_false->copy_src = tv_const(0);
            copy_false->copy_dst = result;
            emit_instr(ctx, copy_false);

            TackyInstr *jump_end = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            jump_end->kind = TACKY_INSTR_JUMP;
            jump_end->jump_target = xstrdup_local(end_label);
            emit_instr(ctx, jump_end);

            TackyInstr *label_true = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            label_true->kind = TACKY_INSTR_LABEL;
            label_true->label = true_label;
            emit_instr(ctx, label_true);

            TackyInstr *copy_true = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            copy_true->kind = TACKY_INSTR_COPY;
            copy_true->copy_src = tv_const(1);
            copy_true->copy_dst = xstrdup_local(result);
            emit_instr(ctx, copy_true);

            TackyInstr *label_end = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            label_end->kind = TACKY_INSTR_LABEL;
            label_end->label = end_label;
            emit_instr(ctx, label_end);

            return tv_var(result, false);
        }
        default:
            return tv_const(0);
    }
}

static void gen_statement(ASTNode *stmt, TackyGenCtx *ctx) {
    if (!stmt) return;
    switch (stmt->type) {
        case AST_STATEMENT_RETURN: {
            TackyInstr *retins = (TackyInstr *)calloc(1, sizeof(TackyInstr));
            retins->kind = TACKY_INSTR_RETURN;
            retins->ret_val = stmt->left ? gen_exp(stmt->left, ctx) : tv_const(0);
            emit_instr(ctx, retins);
            break;
        }
        case AST_STATEMENT_EXPRESSION: {
            TackyVal tmp = gen_exp(stmt->left, ctx);
            if (tmp.kind == TACKY_VAL_VAR && tmp.owns_name && tmp.var_name) {
                free(tmp.var_name);
            }
            break;
        }
        case AST_STATEMENT_NULL:
            break;
        default:
            break;
    }
}

static void gen_declaration(ASTNode *decl, TackyGenCtx *ctx) {
    if (!decl || decl->type != AST_DECLARATION) return;
    if (!decl->left) return; // no initializer

    TackyVal init = gen_exp(decl->left, ctx);
    TackyInstr *copy = (TackyInstr *)calloc(1, sizeof(TackyInstr));
    copy->kind = TACKY_INSTR_COPY;
    copy->copy_src = init;
    copy->copy_dst = xstrdup_local(decl->value);
    emit_instr(ctx, copy);
}

static void gen_block_items(ASTNode *item, TackyGenCtx *ctx) {
    for (ASTNode *curr = item; curr; curr = curr->right) {
        if (!curr || curr->type != AST_BLOCK_ITEM) continue;
        ASTNode *content = curr->left;
        if (!content) continue;
        if (content->type == AST_DECLARATION) {
            gen_declaration(content, ctx);
        } else {
            gen_statement(content, ctx);
        }
    }
}

TackyProgram *tacky_from_ast(ASTNode *ast) {
    if (!ast || ast->type != AST_PROGRAM || !ast->left) return NULL;
    ASTNode *fn = ast->left;
    if (!fn || fn->type != AST_FUNCTION) return NULL;

    TackyGenCtx ctx = {0};
    gen_block_items(fn->left, &ctx);

    TackyInstr *retins = (TackyInstr *)calloc(1, sizeof(TackyInstr));
    retins->kind = TACKY_INSTR_RETURN;
    retins->ret_val = tv_const(0);
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
        case TACKY_UN_NOT: return "Not";
        default: return "?";
    }
}

static const char *binop_name(TackyBinaryOp op) {
    switch (op) {
        case TACKY_BIN_ADD: return "Add";
        case TACKY_BIN_SUB: return "Subtract";
        case TACKY_BIN_MUL: return "Multiply";
        case TACKY_BIN_DIV: return "Divide";
        case TACKY_BIN_REM: return "Remainder";
        case TACKY_BIN_EQUAL: return "Equal";
        case TACKY_BIN_NOT_EQUAL: return "NotEqual";
        case TACKY_BIN_LESS: return "LessThan";
        case TACKY_BIN_LESS_EQUAL: return "LessOrEqual";
        case TACKY_BIN_GREATER: return "GreaterThan";
        case TACKY_BIN_GREATER_EQUAL: return "GreaterOrEqual";
        default: return "?";
    }
}

static void free_tacky_val(TackyVal v) {
    if (v.kind == TACKY_VAL_VAR && v.owns_name && v.var_name) {
        free(v.var_name);
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
                printf("  %s ", binop_name(ins->bin_op));
                if (ins->bin_src1.kind == TACKY_VAL_CONSTANT) printf("%d, ", ins->bin_src1.constant);
                else printf("%s, ", ins->bin_src1.var_name);
                if (ins->bin_src2.kind == TACKY_VAL_CONSTANT) printf("%d ", ins->bin_src2.constant);
                else printf("%s ", ins->bin_src2.var_name);
                printf("-> %s\n", ins->bin_dst);
                break;
            }
            case TACKY_INSTR_COPY:
                printf("  Copy ");
                if (ins->copy_src.kind == TACKY_VAL_CONSTANT) printf("%d", ins->copy_src.constant);
                else printf("%s", ins->copy_src.var_name);
                printf(" -> %s\n", ins->copy_dst);
                break;
            case TACKY_INSTR_JUMP:
                printf("  Jump %s\n", ins->jump_target);
                break;
            case TACKY_INSTR_JUMP_IF_ZERO:
                printf("  JumpIfZero ");
                if (ins->cond_val.kind == TACKY_VAL_CONSTANT) printf("%d", ins->cond_val.constant);
                else printf("%s", ins->cond_val.var_name);
                printf(" -> %s\n", ins->jump_target);
                break;
            case TACKY_INSTR_JUMP_IF_NOT_ZERO:
                printf("  JumpIfNotZero ");
                if (ins->cond_val.kind == TACKY_VAL_CONSTANT) printf("%d", ins->cond_val.constant);
                else printf("%s", ins->cond_val.var_name);
                printf(" -> %s\n", ins->jump_target);
                break;
            case TACKY_INSTR_LABEL:
                printf("  Label %s\n", ins->label);
                break;
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
            printf("\"kind\": \"Binary\", ");
            printf("\"op\": \"%s\", ", binop_name(ins->bin_op));
            printf("\"src1\": ");
            if (ins->bin_src1.kind == TACKY_VAL_CONSTANT) printf("{\"const\": %d}", ins->bin_src1.constant);
            else { printf("{\"var\": \""); json_escape(stdout, ins->bin_src1.var_name); printf("\"}"); }
            printf(", \"src2\": ");
            if (ins->bin_src2.kind == TACKY_VAL_CONSTANT) printf("{\"const\": %d}", ins->bin_src2.constant);
            else { printf("{\"var\": \""); json_escape(stdout, ins->bin_src2.var_name); printf("\"}"); }
            printf(", \"dst\": \""); json_escape(stdout, ins->bin_dst); printf("\"");
        } else if (ins->kind == TACKY_INSTR_COPY) {
            printf("\"kind\": \"Copy\", ");
            printf("\"src\": ");
            if (ins->copy_src.kind == TACKY_VAL_CONSTANT) printf("{\"const\": %d}", ins->copy_src.constant);
            else { printf("{\"var\": \""); json_escape(stdout, ins->copy_src.var_name); printf("\"}"); }
            printf(", \"dst\": \""); json_escape(stdout, ins->copy_dst); printf("\"");
        } else if (ins->kind == TACKY_INSTR_JUMP) {
            printf("\"kind\": \"Jump\", \"target\": \"");
            json_escape(stdout, ins->jump_target);
            printf("\"");
        } else if (ins->kind == TACKY_INSTR_JUMP_IF_ZERO) {
            printf("\"kind\": \"JumpIfZero\", \"condition\": ");
            if (ins->cond_val.kind == TACKY_VAL_CONSTANT) printf("{\"const\": %d}", ins->cond_val.constant);
            else { printf("{\"var\": \""); json_escape(stdout, ins->cond_val.var_name); printf("\"}"); }
            printf(", \"target\": \"");
            json_escape(stdout, ins->jump_target);
            printf("\"");
        } else if (ins->kind == TACKY_INSTR_JUMP_IF_NOT_ZERO) {
            printf("\"kind\": \"JumpIfNotZero\", \"condition\": ");
            if (ins->cond_val.kind == TACKY_VAL_CONSTANT) printf("{\"const\": %d}", ins->cond_val.constant);
            else { printf("{\"var\": \""); json_escape(stdout, ins->cond_val.var_name); printf("\"}"); }
            printf(", \"target\": \"");
            json_escape(stdout, ins->jump_target);
            printf("\"");
        } else if (ins->kind == TACKY_INSTR_LABEL) {
            printf("\"kind\": \"Label\", \"name\": \"");
            json_escape(stdout, ins->label);
            printf("\"");
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
            switch (ins->kind) {
                case TACKY_INSTR_UNARY:
                    free_tacky_val(ins->un_src);
                    free(ins->un_dst);
                    break;
                case TACKY_INSTR_BINARY:
                    free_tacky_val(ins->bin_src1);
                    free_tacky_val(ins->bin_src2);
                    free(ins->bin_dst);
                    break;
                case TACKY_INSTR_COPY:
                    free_tacky_val(ins->copy_src);
                    free(ins->copy_dst);
                    break;
                case TACKY_INSTR_JUMP:
                    free(ins->jump_target);
                    break;
                case TACKY_INSTR_JUMP_IF_ZERO:
                case TACKY_INSTR_JUMP_IF_NOT_ZERO:
                    if (ins->kind != TACKY_INSTR_JUMP) {
                        free_tacky_val(ins->cond_val);
                    }
                    free(ins->jump_target);
                    break;
                case TACKY_INSTR_LABEL:
                    free(ins->label);
                    break;
                case TACKY_INSTR_RETURN:
                    free_tacky_val(ins->ret_val);
                    break;
            }
            free(ins);
            ins = n;
        }
        free(p->fn->name);
        free(p->fn);
    }
    free(p);
}

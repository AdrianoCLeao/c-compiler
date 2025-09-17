#include "../../include/dump/dump.h"
#include "../../include/lexer/lexer.h"
#include "../../include/util/diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)
    #define PATH_SEPARATOR '\\'
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #define MKDIR(path) mkdir(path, 0755)
    #define PATH_SEPARATOR '/'
#endif

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

void dump_ensure_out_dir(void) {
    MKDIR("out");
}

static char *basename_no_ext(const char *path) {
    const char *base = strrchr(path, PATH_SEPARATOR);
    base = base ? base + 1 : path;
    size_t len = strlen(base);
    if (len >= 2) {
        const char *dot = strrchr(base, '.');
        if (dot && strcmp(dot, ".c") == 0) {
            len = (size_t)(dot - base);
        }
    }
    char *name = (char *)malloc(len + 1);
    if (!name) return NULL;
    memcpy(name, base, len);
    name[len] = '\0';
    return name;
}

char *dump_default_path(const char *input_path, const char *ext) {
    dump_ensure_out_dir();
    char *base = basename_no_ext(input_path);
    if (!base) return NULL;
    size_t sz = strlen("out") + 1 + strlen(base) + strlen(ext) + 1;
    char *p = (char *)malloc(sz);
    if (!p) { free(base); return NULL; }
    snprintf(p, sz, "out%c%s%s", PATH_SEPARATOR, base, ext);
    free(base);
    return p;
}

bool dump_tokens_file(const char *input_path, const char *source, const char *out_path) {
    char *path = NULL;
    if (out_path) path = xstrdup(out_path);
    else path = dump_default_path(input_path, ".tokens");
    if (!path) return false;

    FILE *f = fopen(path, "w");
    if (!f) { free(path); return false; }

    Lexer lex; lexer_init(&lex, source);
    size_t i = 0;
    for (;;) {
        Token t = lexer_next_token(&lex);
        int line = 0, col = 0;
        compute_line_col(source, t.start, &line, &col);
        fprintf(f, "%zu\t%s\t\"%s\"\t%d:%d\n", i, token_type_name(t.type), t.value, line, col);
        i++;
        if (t.type == TOKEN_EOF) { free_token(t); break; }
        free_token(t);
    }

    fclose(f);
    free(path);
    return true;
}

static const char *ast_type_name(ASTNodeType t) {
    switch (t) {
        case AST_PROGRAM: return "Program";
        case AST_FUNCTION: return "Function";
        case AST_BLOCK_ITEM: return "BlockItem";
        case AST_DECLARATION: return "Declaration";
        case AST_STATEMENT_RETURN: return "Return";
        case AST_STATEMENT_EXPRESSION: return "ExpressionStmt";
        case AST_STATEMENT_NULL: return "NullStmt";
        case AST_EXPRESSION_CONSTANT: return "Constant";
        case AST_EXPRESSION_VARIABLE: return "Variable";
        case AST_EXPRESSION_ASSIGNMENT: return "Assign";
        case AST_EXPRESSION_NEGATE: return "Negate";
        case AST_EXPRESSION_COMPLEMENT: return "Complement";
        case AST_EXPRESSION_NOT: return "Not";
        case AST_EXPRESSION_ADD: return "Add";
        case AST_EXPRESSION_SUBTRACT: return "Subtract";
        case AST_EXPRESSION_MULTIPLY: return "Multiply";
        case AST_EXPRESSION_DIVIDE: return "Divide";
        case AST_EXPRESSION_REMAINDER: return "Remainder";
        case AST_EXPRESSION_EQUAL: return "Equal";
        case AST_EXPRESSION_NOT_EQUAL: return "NotEqual";
        case AST_EXPRESSION_LESS_THAN: return "LessThan";
        case AST_EXPRESSION_LESS_EQUAL: return "LessOrEqual";
        case AST_EXPRESSION_GREATER_THAN: return "GreaterThan";
        case AST_EXPRESSION_GREATER_EQUAL: return "GreaterOrEqual";
        case AST_EXPRESSION_LOGICAL_AND: return "LogicalAnd";
        case AST_EXPRESSION_LOGICAL_OR: return "LogicalOr";
        default: return "Unknown";
    }
}

static void dump_ast_txt_rec(FILE *f, ASTNode *n, int depth) {
    if (!n) return;
    for (int i = 0; i < depth; i++) fputc(' ', f), fputc(' ', f);
    fprintf(f, "%s", ast_type_name(n->type));
    if (n->value) fprintf(f, ": %s", n->value);
    fputc('\n', f);
    dump_ast_txt_rec(f, n->left, depth + 1);
    dump_ast_txt_rec(f, n->right, depth + 1);
}

static void dump_ast_dot_rec(FILE *f, ASTNode *n, int *counter) {
    if (!n) return;
    int id = (*counter)++;
    // Label
    if (n->value)
        fprintf(f, "  n%d [label=\"%s\\n%s\"];\n", id, ast_type_name(n->type), n->value);
    else
        fprintf(f, "  n%d [label=\"%s\"];\n", id, ast_type_name(n->type));
    int left_id = -1, right_id = -1;
    if (n->left) { left_id = *counter; dump_ast_dot_rec(f, n->left, counter); fprintf(f, "  n%d -> n%d;\n", id, left_id); }
    if (n->right) { right_id = *counter; dump_ast_dot_rec(f, n->right, counter); fprintf(f, "  n%d -> n%d;\n", id, right_id); }
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

static void dump_ast_json_rec(FILE *f, ASTNode *n) {
    if (!n) { fputs("null", f); return; }
    fputs("{\n", f);
    fprintf(f, "  \"type\": \"%s\"", ast_type_name(n->type));
    if (n->value) {
        fputs(",\n  \"value\": \"", f); json_escape(f, n->value); fputs("\"", f);
    }
    fputs(",\n  \"left\": ", f); dump_ast_json_rec(f, n->left);
    fputs(",\n  \"right\": ", f); dump_ast_json_rec(f, n->right);
    fputs("\n}", f);
}

bool dump_ast_file(ASTNode *ast, const char *input_path, DumpAstFormat fmt, const char *out_path) {
    const char *ext = ".ast.txt";
    if (fmt == DUMP_AST_DOT) ext = ".ast.dot";
    else if (fmt == DUMP_AST_JSON) ext = ".ast.json";

    char *path = NULL;
    if (out_path) path = (char *)xstrdup(out_path);
    else path = dump_default_path(input_path, ext);
    if (!path) return false;

    FILE *f = fopen(path, "w");
    if (!f) { free(path); return false; }

    switch (fmt) {
        case DUMP_AST_TXT:
            dump_ast_txt_rec(f, ast, 0);
            break;
        case DUMP_AST_DOT: {
            fputs("digraph AST {\n", f);
            int counter = 0;
            dump_ast_dot_rec(f, ast, &counter);
            fputs("}\n", f);
            break;
        }
        case DUMP_AST_JSON:
            dump_ast_json_rec(f, ast);
            fputc('\n', f);
            break;
        default:
            fclose(f); free(path); return false;
    }

    fclose(f);
    free(path);
    return true;
}

bool dump_tacky_file(TackyProgram *p, const char *input_path, DumpTackyFormat fmt, const char *out_path) {
    const char *ext = (fmt == DUMP_TACKY_JSON) ? ".tacky.json" : ".tacky.txt";
    char *path = NULL;
    if (out_path) path = (char *)xstrdup(out_path);
    else path = dump_default_path(input_path, ext);
    if (!path) return false;
    FILE *f = fopen(path, "w");
    if (!f) { free(path); return false; }

    if (fmt == DUMP_TACKY_JSON) {
        fprintf(f, "{\n  \"function\": \"%s\",\n  \"body\": [\n", p->fn ? p->fn->name : "");
        int first = 1;
        for (TackyInstr *ins = p->fn ? p->fn->body : NULL; ins; ins = ins->next) {
            if (!first) fprintf(f, ",\n");
            first = 0;
            fprintf(f, "    {");
            if (ins->kind == TACKY_INSTR_UNARY) {
                const char *op = (ins->un_op == TACKY_UN_NEGATE) ? "Negate" : (ins->un_op == TACKY_UN_COMPLEMENT ? "Complement" : "Not");
                fprintf(f, "\"kind\": \"Unary\", \"op\": \"%s\", \"src\": ", op);
                if (ins->un_src.kind == TACKY_VAL_CONSTANT) fprintf(f, "{\"const\": %d}", ins->un_src.constant);
                else fprintf(f, "{\"var\": \"%s\"}", ins->un_src.var_name);
                fprintf(f, ", \"dst\": \"%s\"", ins->un_dst);
            } else if (ins->kind == TACKY_INSTR_BINARY) {
                const char *op = "?";
                switch (ins->bin_op) {
                    case TACKY_BIN_ADD: op = "Add"; break;
                    case TACKY_BIN_SUB: op = "Subtract"; break;
                    case TACKY_BIN_MUL: op = "Multiply"; break;
                    case TACKY_BIN_DIV: op = "Divide"; break;
                    case TACKY_BIN_REM: op = "Remainder"; break;
                    case TACKY_BIN_EQUAL: op = "Equal"; break;
                    case TACKY_BIN_NOT_EQUAL: op = "NotEqual"; break;
                    case TACKY_BIN_LESS: op = "LessThan"; break;
                    case TACKY_BIN_LESS_EQUAL: op = "LessOrEqual"; break;
                    case TACKY_BIN_GREATER: op = "GreaterThan"; break;
                    case TACKY_BIN_GREATER_EQUAL: op = "GreaterOrEqual"; break;
                }
                fprintf(f, "\"kind\": \"Binary\", \"op\": \"%s\", \"src1\": ", op);
                if (ins->bin_src1.kind == TACKY_VAL_CONSTANT) fprintf(f, "{\"const\": %d}", ins->bin_src1.constant);
                else fprintf(f, "{\"var\": \"%s\"}", ins->bin_src1.var_name);
                fprintf(f, ", \"src2\": ");
                if (ins->bin_src2.kind == TACKY_VAL_CONSTANT) fprintf(f, "{\"const\": %d}", ins->bin_src2.constant);
                else fprintf(f, "{\"var\": \"%s\"}", ins->bin_src2.var_name);
                fprintf(f, ", \"dst\": \"%s\"", ins->bin_dst);
            } else if (ins->kind == TACKY_INSTR_COPY) {
                fprintf(f, "\"kind\": \"Copy\", \"src\": ");
                if (ins->copy_src.kind == TACKY_VAL_CONSTANT) fprintf(f, "{\"const\": %d}", ins->copy_src.constant);
                else fprintf(f, "{\"var\": \"%s\"}", ins->copy_src.var_name);
                fprintf(f, ", \"dst\": \"%s\"", ins->copy_dst);
            } else if (ins->kind == TACKY_INSTR_JUMP) {
                fprintf(f, "\"kind\": \"Jump\", \"target\": \"%s\"", ins->jump_target);
            } else if (ins->kind == TACKY_INSTR_JUMP_IF_ZERO) {
                fprintf(f, "\"kind\": \"JumpIfZero\", \"condition\": ");
                if (ins->cond_val.kind == TACKY_VAL_CONSTANT) fprintf(f, "{\"const\": %d}", ins->cond_val.constant);
                else fprintf(f, "{\"var\": \"%s\"}", ins->cond_val.var_name);
                fprintf(f, ", \"target\": \"%s\"", ins->jump_target);
            } else if (ins->kind == TACKY_INSTR_JUMP_IF_NOT_ZERO) {
                fprintf(f, "\"kind\": \"JumpIfNotZero\", \"condition\": ");
                if (ins->cond_val.kind == TACKY_VAL_CONSTANT) fprintf(f, "{\"const\": %d}", ins->cond_val.constant);
                else fprintf(f, "{\"var\": \"%s\"}", ins->cond_val.var_name);
                fprintf(f, ", \"target\": \"%s\"", ins->jump_target);
            } else if (ins->kind == TACKY_INSTR_LABEL) {
                fprintf(f, "\"kind\": \"Label\", \"name\": \"%s\"", ins->label);
            } else if (ins->kind == TACKY_INSTR_RETURN) {
                fprintf(f, "\"kind\": \"Return\", \"value\": ");
                if (ins->ret_val.kind == TACKY_VAL_CONSTANT) fprintf(f, "{\"const\": %d}", ins->ret_val.constant);
                else fprintf(f, "{\"var\": \"%s\"}", ins->ret_val.var_name);
            }
            fprintf(f, "}");
        }
        fprintf(f, "\n  ]\n}\n");
    } else {
        fprintf(f, "Function %s()\n", p->fn ? p->fn->name : "");
        for (TackyInstr *ins = p->fn ? p->fn->body : NULL; ins; ins = ins->next) {
            switch (ins->kind) {
                case TACKY_INSTR_UNARY: {
                    const char *op = (ins->un_op == TACKY_UN_NEGATE) ? "Negate" : (ins->un_op == TACKY_UN_COMPLEMENT ? "Complement" : "Not");
                    if (ins->un_src.kind == TACKY_VAL_CONSTANT)
                        fprintf(f, "  %s %d -> %s\n", op, ins->un_src.constant, ins->un_dst);
                    else
                        fprintf(f, "  %s %s -> %s\n", op, ins->un_src.var_name, ins->un_dst);
                    break;
                }
                case TACKY_INSTR_BINARY: {
                    const char *op = "?";
                    switch (ins->bin_op) {
                        case TACKY_BIN_ADD: op = "Add"; break;
                        case TACKY_BIN_SUB: op = "Subtract"; break;
                        case TACKY_BIN_MUL: op = "Multiply"; break;
                        case TACKY_BIN_DIV: op = "Divide"; break;
                        case TACKY_BIN_REM: op = "Remainder"; break;
                        case TACKY_BIN_EQUAL: op = "Equal"; break;
                        case TACKY_BIN_NOT_EQUAL: op = "NotEqual"; break;
                        case TACKY_BIN_LESS: op = "LessThan"; break;
                        case TACKY_BIN_LESS_EQUAL: op = "LessOrEqual"; break;
                        case TACKY_BIN_GREATER: op = "GreaterThan"; break;
                        case TACKY_BIN_GREATER_EQUAL: op = "GreaterOrEqual"; break;
                    }
                    fprintf(f, "  %s ", op);
                    if (ins->bin_src1.kind == TACKY_VAL_CONSTANT) fprintf(f, "%d, ", ins->bin_src1.constant);
                    else fprintf(f, "%s, ", ins->bin_src1.var_name);
                    if (ins->bin_src2.kind == TACKY_VAL_CONSTANT) fprintf(f, "%d ", ins->bin_src2.constant);
                    else fprintf(f, "%s ", ins->bin_src2.var_name);
                    fprintf(f, "-> %s\n", ins->bin_dst);
                    break;
                }
                case TACKY_INSTR_COPY:
                    fprintf(f, "  Copy ");
                    if (ins->copy_src.kind == TACKY_VAL_CONSTANT) fprintf(f, "%d", ins->copy_src.constant);
                    else fprintf(f, "%s", ins->copy_src.var_name);
                    fprintf(f, " -> %s\n", ins->copy_dst);
                    break;
                case TACKY_INSTR_JUMP:
                    fprintf(f, "  Jump %s\n", ins->jump_target);
                    break;
                case TACKY_INSTR_JUMP_IF_ZERO:
                    fprintf(f, "  JumpIfZero ");
                    if (ins->cond_val.kind == TACKY_VAL_CONSTANT) fprintf(f, "%d", ins->cond_val.constant);
                    else fprintf(f, "%s", ins->cond_val.var_name);
                    fprintf(f, " -> %s\n", ins->jump_target);
                    break;
                case TACKY_INSTR_JUMP_IF_NOT_ZERO:
                    fprintf(f, "  JumpIfNotZero ");
                    if (ins->cond_val.kind == TACKY_VAL_CONSTANT) fprintf(f, "%d", ins->cond_val.constant);
                    else fprintf(f, "%s", ins->cond_val.var_name);
                    fprintf(f, " -> %s\n", ins->jump_target);
                    break;
                case TACKY_INSTR_LABEL:
                    fprintf(f, "  Label %s\n", ins->label);
                    break;
                case TACKY_INSTR_RETURN:
                    if (ins->ret_val.kind == TACKY_VAL_CONSTANT)
                        fprintf(f, "  Return %d\n", ins->ret_val.constant);
                    else
                        fprintf(f, "  Return %s\n", ins->ret_val.var_name);
                    break;
            }
        }
    }
    fclose(f);
    free(path);
    return true;
}

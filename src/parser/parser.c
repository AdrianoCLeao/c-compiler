#include "../../include/parser/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/util/diag.h"

static ASTNode *create_ast_node(ASTNodeType type, const char *value, ASTNode *left, ASTNode *right) {
    ASTNode *node = (ASTNode *)malloc(sizeof(ASTNode));
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->owns_value = value != NULL;
    node->left = left;
    node->right = right;
    node->third = NULL;
    node->fourth = NULL;
    return node;
}

static void consume(Parser *parser, LexTokenType expected_type) {
    if (parser->current_token.type != expected_type) {
        int line = 0, col = 0;
        compute_line_col(parser->lexer->input, parser->current_token.start, &line, &col);
        fprintf(stderr,
                "Syntax Error at %d:%d: Expected %s but got %s ('%s')\n",
                line, col,
                token_type_name(expected_type),
                token_type_name(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "");
        exit(1);
    }
    free_token(parser->current_token);
    parser->current_token = lexer_next_token(parser->lexer);
}

void parser_init(Parser *parser, Lexer *lexer) {
    parser->lexer = lexer;
    parser->current_token = lexer_next_token(lexer);
}

static ASTNode *parse_function(Parser *parser);
static ASTNode *parse_block(Parser *parser);
static ASTNode *parse_expression(Parser *parser);
static ASTNode *parse_conditional(Parser *parser);
static ASTNode *parse_block_item(Parser *parser);
static ASTNode *parse_declaration(Parser *parser);
static ASTNode *parse_statement(Parser *parser);
static ASTNode *parse_for_statement(Parser *parser);
static ASTNode *wrap_expression_statement(ASTNode *expr);

ASTNode *parse_program(Parser *parser) {
    return create_ast_node(AST_PROGRAM, NULL, parse_function(parser), NULL);
}

static void append_block_item(ASTNode **head, ASTNode **tail, ASTNode *item) {
    if (!item) return;
    if (!*head) {
        *head = *tail = item;
    } else {
        (*tail)->right = item;
        *tail = item;
    }
}

ASTNode *parse_function(Parser *parser) {
    consume(parser, TOKEN_KEYWORD_INT);

    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        int line = 0, col = 0;
        compute_line_col(parser->lexer->input, parser->current_token.start, &line, &col);
        fprintf(stderr, "Syntax Error at %d:%d: Expected function name, got %s ('%s')\n",
                line, col,
                token_type_name(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "");
        exit(1);
    }
    char *func_name_copy = strdup(parser->current_token.value);
    consume(parser, TOKEN_IDENTIFIER);

    consume(parser, TOKEN_OPEN_PAREN);
    consume(parser, TOKEN_KEYWORD_VOID);
    consume(parser, TOKEN_CLOSE_PAREN);

    consume(parser, TOKEN_OPEN_BRACE);
    ASTNode *block_head = parse_block(parser);
    ASTNode *function = create_ast_node(AST_FUNCTION, func_name_copy, block_head, NULL);
    free(func_name_copy);
    return function;
}

static ASTNode *parse_block_item(Parser *parser) {
    if (parser->current_token.type == TOKEN_KEYWORD_INT) {
        ASTNode *decl = parse_declaration(parser);
        return create_ast_node(AST_BLOCK_ITEM, NULL, decl, NULL);
    }
    ASTNode *stmt = parse_statement(parser);
    return create_ast_node(AST_BLOCK_ITEM, NULL, stmt, NULL);
}

static ASTNode *parse_block(Parser *parser) {
    ASTNode *block_head = NULL;
    ASTNode *block_tail = NULL;
    while (parser->current_token.type != TOKEN_CLOSE_BRACE) {
        ASTNode *item = parse_block_item(parser);
        append_block_item(&block_head, &block_tail, item);
    }
    consume(parser, TOKEN_CLOSE_BRACE);
    return block_head;
}

static ASTNode *parse_declaration(Parser *parser) {
    consume(parser, TOKEN_KEYWORD_INT);

    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        int line = 0, col = 0;
        compute_line_col(parser->lexer->input, parser->current_token.start, &line, &col);
        fprintf(stderr, "Syntax Error at %d:%d: Expected identifier in declaration, got %s ('%s')\n",
                line, col,
                token_type_name(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "");
        exit(1);
    }
    char *name_copy = strdup(parser->current_token.value);
    consume(parser, TOKEN_IDENTIFIER);

    ASTNode *init = NULL;
    if (parser->current_token.type == TOKEN_ASSIGN) {
        consume(parser, TOKEN_ASSIGN);
        init = parse_expression(parser);
    }
    consume(parser, TOKEN_SEMICOLON);

    ASTNode *decl = create_ast_node(AST_DECLARATION, name_copy, init, NULL);
    free(name_copy);
    return decl;
}

static ASTNode *wrap_expression_statement(ASTNode *expr) {
    if (!expr) return NULL;
    return create_ast_node(AST_STATEMENT_EXPRESSION, NULL, expr, NULL);
}

static ASTNode *parse_for_statement(Parser *parser) {
    consume(parser, TOKEN_KEYWORD_FOR);
    consume(parser, TOKEN_OPEN_PAREN);

    ASTNode *init = NULL;
    if (parser->current_token.type == TOKEN_SEMICOLON) {
        consume(parser, TOKEN_SEMICOLON);
    } else if (parser->current_token.type == TOKEN_KEYWORD_INT) {
        init = parse_declaration(parser);
    } else {
        ASTNode *expr = parse_expression(parser);
        consume(parser, TOKEN_SEMICOLON);
        init = wrap_expression_statement(expr);
    }

    ASTNode *condition = NULL;
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        condition = parse_expression(parser);
    }
    consume(parser, TOKEN_SEMICOLON);

    ASTNode *post = NULL;
    if (parser->current_token.type != TOKEN_CLOSE_PAREN) {
        post = parse_expression(parser);
    }
    consume(parser, TOKEN_CLOSE_PAREN);

    ASTNode *body = parse_statement(parser);

    ASTNode *for_node = create_ast_node(AST_STATEMENT_FOR, NULL, init, condition);
    for_node->third = post;
    for_node->fourth = body;
    return for_node;
}

static ASTNode *parse_statement(Parser *parser) {
    if (parser->current_token.type == TOKEN_KEYWORD_RETURN) {
        consume(parser, TOKEN_KEYWORD_RETURN);
        ASTNode *expr = parse_expression(parser);
        consume(parser, TOKEN_SEMICOLON);
        return create_ast_node(AST_STATEMENT_RETURN, NULL, expr, NULL);
    }

    if (parser->current_token.type == TOKEN_OPEN_BRACE) {
        consume(parser, TOKEN_OPEN_BRACE);
        ASTNode *block = parse_block(parser);
        return create_ast_node(AST_STATEMENT_COMPOUND, NULL, block, NULL);
    }

    if (parser->current_token.type == TOKEN_KEYWORD_IF) {
        consume(parser, TOKEN_KEYWORD_IF);
        consume(parser, TOKEN_OPEN_PAREN);
        ASTNode *condition = parse_expression(parser);
        consume(parser, TOKEN_CLOSE_PAREN);
        ASTNode *then_stmt = parse_statement(parser);
        ASTNode *else_stmt = NULL;
        if (parser->current_token.type == TOKEN_KEYWORD_ELSE) {
            consume(parser, TOKEN_KEYWORD_ELSE);
            else_stmt = parse_statement(parser);
        }
        ASTNode *if_node = create_ast_node(AST_STATEMENT_IF, NULL, condition, then_stmt);
        if_node->third = else_stmt;
        return if_node;
    }

    if (parser->current_token.type == TOKEN_KEYWORD_WHILE) {
        consume(parser, TOKEN_KEYWORD_WHILE);
        consume(parser, TOKEN_OPEN_PAREN);
        ASTNode *condition = parse_expression(parser);
        consume(parser, TOKEN_CLOSE_PAREN);
        ASTNode *body = parse_statement(parser);
        ASTNode *while_node = create_ast_node(AST_STATEMENT_WHILE, NULL, condition, body);
        return while_node;
    }

    if (parser->current_token.type == TOKEN_KEYWORD_DO) {
        consume(parser, TOKEN_KEYWORD_DO);
        ASTNode *body = parse_statement(parser);
        consume(parser, TOKEN_KEYWORD_WHILE);
        consume(parser, TOKEN_OPEN_PAREN);
        ASTNode *condition = parse_expression(parser);
        consume(parser, TOKEN_CLOSE_PAREN);
        consume(parser, TOKEN_SEMICOLON);
        ASTNode *do_node = create_ast_node(AST_STATEMENT_DO_WHILE, NULL, body, condition);
        return do_node;
    }

    if (parser->current_token.type == TOKEN_KEYWORD_FOR) {
        return parse_for_statement(parser);
    }

    if (parser->current_token.type == TOKEN_KEYWORD_BREAK) {
        consume(parser, TOKEN_KEYWORD_BREAK);
        consume(parser, TOKEN_SEMICOLON);
        return create_ast_node(AST_STATEMENT_BREAK, NULL, NULL, NULL);
    }

    if (parser->current_token.type == TOKEN_KEYWORD_CONTINUE) {
        consume(parser, TOKEN_KEYWORD_CONTINUE);
        consume(parser, TOKEN_SEMICOLON);
        return create_ast_node(AST_STATEMENT_CONTINUE, NULL, NULL, NULL);
    }

    if (parser->current_token.type == TOKEN_SEMICOLON) {
        consume(parser, TOKEN_SEMICOLON);
        return create_ast_node(AST_STATEMENT_NULL, NULL, NULL, NULL);
    }

    ASTNode *expr = parse_expression(parser);
    consume(parser, TOKEN_SEMICOLON);
    return create_ast_node(AST_STATEMENT_EXPRESSION, NULL, expr, NULL);
}

static int precedence(LexTokenType t) {
    switch (t) {
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
            return 50;
        case TOKEN_PLUS:
        case TOKEN_NEGATION:
            return 45;
        case TOKEN_LESS:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQUAL:
            return 35;
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_NOT_EQUAL:
            return 30;
        case TOKEN_AMP_AMP:
            return 10;
        case TOKEN_PIPE_PIPE:
            return 5;
        case TOKEN_ASSIGN:
            return 1;
        default:
            return -1;
    }
}

static ASTNodeType binop_node_type(LexTokenType t) {
    switch (t) {
        case TOKEN_PLUS: return AST_EXPRESSION_ADD;
        case TOKEN_NEGATION: return AST_EXPRESSION_SUBTRACT;
        case TOKEN_STAR: return AST_EXPRESSION_MULTIPLY;
        case TOKEN_SLASH: return AST_EXPRESSION_DIVIDE;
        case TOKEN_PERCENT: return AST_EXPRESSION_REMAINDER;
        case TOKEN_EQUAL_EQUAL: return AST_EXPRESSION_EQUAL;
        case TOKEN_NOT_EQUAL: return AST_EXPRESSION_NOT_EQUAL;
        case TOKEN_LESS: return AST_EXPRESSION_LESS_THAN;
        case TOKEN_LESS_EQUAL: return AST_EXPRESSION_LESS_EQUAL;
        case TOKEN_GREATER: return AST_EXPRESSION_GREATER_THAN;
        case TOKEN_GREATER_EQUAL: return AST_EXPRESSION_GREATER_EQUAL;
        case TOKEN_AMP_AMP: return AST_EXPRESSION_LOGICAL_AND;
        case TOKEN_PIPE_PIPE: return AST_EXPRESSION_LOGICAL_OR;
        default: return AST_EXPRESSION_ADD;
    }
}

static ASTNode *parse_factor(Parser *parser);

static ASTNode *parse_binary_expr(Parser *parser, int min_prec) {
    ASTNode *left = parse_factor(parser);

    for (;;) {
        LexTokenType op_tok = parser->current_token.type;
        int prec = precedence(op_tok);
        if (prec < min_prec) break;

        if (op_tok == TOKEN_ASSIGN) {
            consume(parser, TOKEN_ASSIGN);
            ASTNode *right = parse_binary_expr(parser, prec);
            left = create_ast_node(AST_EXPRESSION_ASSIGNMENT, NULL, left, right);
            continue;
        }

        consume(parser, op_tok);
        ASTNode *right = parse_binary_expr(parser, prec + 1);
        left = create_ast_node(binop_node_type(op_tok), NULL, left, right);
    }

    return left;
}

static ASTNode *parse_factor(Parser *parser) {
    if (parser->current_token.type == TOKEN_CONSTANT) {
        ASTNode *constant = create_ast_node(AST_EXPRESSION_CONSTANT, parser->current_token.value, NULL, NULL);
        consume(parser, TOKEN_CONSTANT);
        return constant;
    }

    if (parser->current_token.type == TOKEN_IDENTIFIER) {
        ASTNode *var = create_ast_node(AST_EXPRESSION_VARIABLE, parser->current_token.value, NULL, NULL);
        consume(parser, TOKEN_IDENTIFIER);
        return var;
    }

    if (parser->current_token.type == TOKEN_NEGATION ||
        parser->current_token.type == TOKEN_TILDE ||
        parser->current_token.type == TOKEN_NOT) {
        LexTokenType op = parser->current_token.type;
        consume(parser, op);
        ASTNode *inner_expr = parse_factor(parser);
        ASTNodeType node_type = AST_EXPRESSION_NEGATE;
        if (op == TOKEN_TILDE) {
            node_type = AST_EXPRESSION_COMPLEMENT;
        } else if (op == TOKEN_NOT) {
            node_type = AST_EXPRESSION_NOT;
        }
        return create_ast_node(node_type, NULL, inner_expr, NULL);
    }

    if (parser->current_token.type == TOKEN_OPEN_PAREN) {
        consume(parser, TOKEN_OPEN_PAREN);
        ASTNode *inner_expr = parse_binary_expr(parser, 1);
        consume(parser, TOKEN_CLOSE_PAREN);
        return inner_expr;
    }

    int line = 0, col = 0;
    compute_line_col(parser->lexer->input, parser->current_token.start, &line, &col);
    fprintf(stderr, "Syntax Error at %d:%d: Expected an expression, got %s ('%s')\n",
            line, col,
            token_type_name(parser->current_token.type),
            parser->current_token.value ? parser->current_token.value : "");
    exit(1);
}

ASTNode *parse_expression(Parser *parser) {
    return parse_conditional(parser);
}

static ASTNode *parse_conditional(Parser *parser) {
    ASTNode *condition = parse_binary_expr(parser, 1);
    if (parser->current_token.type == TOKEN_QUESTION) {
        consume(parser, TOKEN_QUESTION);
        ASTNode *if_true = parse_expression(parser);
        consume(parser, TOKEN_COLON);
        ASTNode *if_false = parse_expression(parser);
        ASTNode *cond = create_ast_node(AST_EXPRESSION_CONDITIONAL, NULL, condition, if_true);
        cond->third = if_false;
        return cond;
    }
    return condition;
}

void free_ast(ASTNode *node) {
    if (!node) return;
    free_ast(node->left);
    free_ast(node->right);
    free_ast(node->third);
    free_ast(node->fourth);
    if (node->value && node->owns_value) {
        free(node->value);
    }
    free(node);
}

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
}

void print_ast(ASTNode *node, int depth) {
    if (!node) return;

    print_indent(depth);
    switch (node->type) {
        case AST_PROGRAM:
            printf("Program\n");
            break;
        case AST_FUNCTION:
            printf("Function: %s\n", node->value);
            break;
        case AST_BLOCK_ITEM:
            printf("BlockItem\n");
            break;
        case AST_DECLARATION:
            printf("Declaration: %s\n", node->value);
            break;
        case AST_STATEMENT_RETURN:
            printf("Return\n");
            break;
        case AST_STATEMENT_EXPRESSION:
            printf("ExpressionStmt\n");
            break;
        case AST_STATEMENT_NULL:
            printf("NullStmt\n");
            break;
        case AST_STATEMENT_IF:
            printf("If\n");
            break;
        case AST_STATEMENT_COMPOUND:
            printf("Compound\n");
            break;
        case AST_STATEMENT_WHILE:
            printf("While\n");
            break;
        case AST_STATEMENT_DO_WHILE:
            printf("DoWhile\n");
            break;
        case AST_STATEMENT_FOR:
            printf("For\n");
            break;
        case AST_STATEMENT_BREAK:
            printf("Break\n");
            break;
        case AST_STATEMENT_CONTINUE:
            printf("Continue\n");
            break;
        case AST_EXPRESSION_CONSTANT:
            printf("Constant: %s\n", node->value);
            break;
        case AST_EXPRESSION_VARIABLE:
            printf("Variable: %s\n", node->value);
            break;
        case AST_EXPRESSION_ASSIGNMENT:
            printf("Assign\n");
            break;
        case AST_EXPRESSION_CONDITIONAL:
            printf("Conditional\n");
            break;
        case AST_EXPRESSION_NEGATE:
            printf("Negate\n");
            break;
        case AST_EXPRESSION_COMPLEMENT:
            printf("Complement\n");
            break;
        case AST_EXPRESSION_NOT:
            printf("Not\n");
            break;
        case AST_EXPRESSION_ADD:
            printf("Add\n");
            break;
        case AST_EXPRESSION_SUBTRACT:
            printf("Subtract\n");
            break;
        case AST_EXPRESSION_MULTIPLY:
            printf("Multiply\n");
            break;
        case AST_EXPRESSION_DIVIDE:
            printf("Divide\n");
            break;
        case AST_EXPRESSION_REMAINDER:
            printf("Remainder\n");
            break;
        case AST_EXPRESSION_EQUAL:
            printf("Equal\n");
            break;
        case AST_EXPRESSION_NOT_EQUAL:
            printf("NotEqual\n");
            break;
        case AST_EXPRESSION_LESS_THAN:
            printf("LessThan\n");
            break;
        case AST_EXPRESSION_LESS_EQUAL:
            printf("LessOrEqual\n");
            break;
        case AST_EXPRESSION_GREATER_THAN:
            printf("GreaterThan\n");
            break;
        case AST_EXPRESSION_GREATER_EQUAL:
            printf("GreaterOrEqual\n");
            break;
        case AST_EXPRESSION_LOGICAL_AND:
            printf("LogicalAnd\n");
            break;
        case AST_EXPRESSION_LOGICAL_OR:
            printf("LogicalOr\n");
            break;
        default:
            printf("Unknown Node\n");
            break;
    }

    print_ast(node->left, depth + 1);
    print_ast(node->right, depth + 1);
    print_ast(node->third, depth + 1);
    print_ast(node->fourth, depth + 1);
}

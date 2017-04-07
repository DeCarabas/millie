#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

struct Expression *MakeSyntaxError(struct Arena *arena, uint32_t position)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_ERROR;
    result->start_token = position;
    result->end_token = position;
    return result;
}

struct Expression *MakeLambda(struct Arena *arena, uint32_t start_token,
                              Symbol variable, struct Expression *body)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_LAMBDA;
    result->start_token = start_token;
    result->end_token = body->end_token;
    result->lambda_id = variable;
    result->lambda_body = body;
    return result;
}

struct Expression *MakeIdentifier(struct Arena *arena, uint32_t token_pos,
                                  Symbol id)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_IDENTIFIER;
    result->start_token = result->end_token = token_pos;
    result->identifier_id = id;
    return result;
}

struct Expression *MakeApply(struct Arena *arena, struct Expression *func_expr,
                             struct Expression *arg_expr)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_APPLY;
    result->start_token = func_expr->start_token;
    result->end_token = arg_expr->end_token;
    result->apply_function = func_expr;
    result->apply_argument = arg_expr;
    return result;
}

struct Expression *MakeLet(struct Arena *arena, uint32_t let_pos,
                           Symbol variable, struct Expression *value,
                           struct Expression *body)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_LET;
    result->start_token = let_pos;
    result->end_token = body->end_token;
    result->let_id = variable;
    result->let_value = value;
    result->let_body = body;
    return result;
}

struct Expression *MakeLetRec(struct Arena *arena, uint32_t let_pos,
                              Symbol variable, struct Expression *value,
                              struct Expression *body)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_LETREC;
    result->start_token = let_pos;
    result->end_token = body->end_token;
    result->let_id = variable;
    result->let_value = value;
    result->let_body = body;
    return result;
}

struct Expression *MakeIf(struct Arena *arena, uint32_t if_pos,
                          struct Expression *test,
                          struct Expression *then_branch,
                          struct Expression *else_branch)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_IF;
    result->start_token = if_pos;
    result->end_token = else_branch->end_token;
    result->if_test = test;
    result->if_then = then_branch;
    result->if_else = else_branch;
    return result;
}

struct Expression *MakeBinary(struct Arena *arena, MILLIE_TOKEN op,
                              struct Expression *left,
                              struct Expression *right)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_BINARY;
    result->start_token = left->start_token;
    result->end_token = right->end_token;
    result->binary_operator = op;
    result->binary_left = left;
    result->binary_right = right;
    return result;
}

struct Expression *MakeUnary(struct Arena *arena, uint32_t operator_pos,
                             MILLIE_TOKEN op, struct Expression *arg)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_UNARY;
    result->start_token = operator_pos;
    result->end_token = arg->end_token;
    result->unary_operator = op;
    result->unary_arg = arg;
    return result;
}

struct Expression *MakeBooleanLiteral(struct Arena *arena, uint32_t pos,
                                      bool value)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = value ? EXP_TRUE : EXP_FALSE;
    result->start_token = result->end_token = pos;
    return result;
}

struct Expression *MakeIntegerLiteral(struct Arena *arena, uint32_t pos,
                                      uint64_t value)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_INTEGER_CONSTANT;
    result->literal_value = value;
    result->start_token = result->end_token = pos;
    return result;
}

struct Expression *MakeTuple(struct Arena *arena, struct Expression *first,
                             struct Expression *next)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_TUPLE;
    result->tuple_first = first;
    result->tuple_next = next;
    result->start_token = first->start_token;
    result->end_token = next->end_token;
    return result;
}

static void _PrintIndent(int indent)
{
    for(int i = 0; i < indent; i++) {
        printf("  ");
    }
}

static void _DumpExprImpl(
    struct SymbolTable *table,
    struct MillieTokens *tokens,
    struct Expression *expression,
    int indent
)
{
    switch(expression->type) {
    case EXP_LAMBDA:
        {
            struct MString *id = FindSymbolKey(table, expression->lambda_id);
            _PrintIndent(indent); printf("lambda %s =>\n", MStringData(id));
            _DumpExprImpl(table, tokens, expression->lambda_body, indent+1);
            MStringFree(&id);
        }
        break;

    case EXP_IDENTIFIER:
        {
            struct MString *id = FindSymbolKey(table, expression->identifier_id);
            _PrintIndent(indent); printf("id %s\n", MStringData(id));
            MStringFree(&id);
        }
        break;

    case EXP_APPLY:
        {
            _PrintIndent(indent); printf("apply\n");
            _DumpExprImpl(table, tokens, expression->apply_function, indent+1);
            _DumpExprImpl(table, tokens, expression->apply_argument, indent+1);
        }
        break;

    case EXP_LET:
        {
            struct MString *id = FindSymbolKey(table, expression->let_id);
            _PrintIndent(indent); printf("let %s = \n", MStringData(id));
            _DumpExprImpl(table, tokens, expression->let_value, indent+1);
            _PrintIndent(indent); printf("in\n");
            _DumpExprImpl(table, tokens, expression->let_body, indent+1);
            MStringFree(&id);
        }
        break;
    case EXP_LETREC:
        {
            struct MString *id = FindSymbolKey(table, expression->let_id);
            _PrintIndent(indent); printf("let rec %s = \n", MStringData(id));
            _DumpExprImpl(table, tokens, expression->let_value, indent+1);
            _PrintIndent(indent); printf("in\n");
            _DumpExprImpl(table, tokens, expression->let_body, indent+1);
            MStringFree(&id);
        }
        break;

    case EXP_INTEGER_CONSTANT:
        {
            _PrintIndent(indent);
            printf("literal %llu\n", expression->literal_value);
        }
        break;

    case EXP_TRUE:
        {
            _PrintIndent(indent); printf("true\n");
        }
        break;

    case EXP_FALSE:
        {
            _PrintIndent(indent); printf("false\n");
        }
        break;

    case EXP_IF:
        {
            _PrintIndent(indent); printf("if\n");
            _DumpExprImpl(table, tokens, expression->if_test, indent+1);
            _PrintIndent(indent); printf("then\n");
            _DumpExprImpl(table, tokens, expression->if_then, indent+1);
            _PrintIndent(indent); printf("else\n");
            _DumpExprImpl(table, tokens, expression->if_else, indent+1);
        }
        break;

    case EXP_BINARY:
        {
            uint32_t bin_tok = expression->binary_left->end_token + 1;
            struct MString *operator = ExtractToken(tokens, bin_tok);
            _PrintIndent(indent); printf("binary %s\n", MStringData(operator));
            _DumpExprImpl(table, tokens, expression->binary_left, indent+1);
            _DumpExprImpl(table, tokens, expression->binary_right, indent+1);
            MStringFree(&operator);
        }
        break;

    case EXP_UNARY:
        {
            struct MString *operator;
            operator = ExtractToken(tokens, expression->start_token);
            _PrintIndent(indent); printf("unary %s\n", MStringData(operator));
            _DumpExprImpl(table, tokens, expression->unary_arg, indent+1);
            MStringFree(&operator);
        }
        break;

    case EXP_TUPLE:
        {
            _PrintIndent(indent); printf("tuple\n");
            while(expression->type == EXP_TUPLE) {
                _DumpExprImpl(table, tokens, expression->tuple_first, indent+1);
                expression = expression->tuple_next;
            }
            _DumpExprImpl(table, tokens, expression, indent+1);
        }
        break;

    case EXP_ERROR:
        _PrintIndent(indent); printf("ERROR");
        break;

    case EXP_INVALID:
        _PrintIndent(indent); printf("???\n");
        break;
    }
}

void DumpExpression(struct SymbolTable *table, struct MillieTokens *tokens,
                    struct Expression *expression)
{
    _DumpExprImpl(table, tokens, expression, 0);
}

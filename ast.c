#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

struct Expression *MakeLambda(struct Arena *arena, Symbol variable,
                              struct Expression *body)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_LAMBDA;
    result->lambda_id = variable;
    result->lambda_body = body;
    return result;
}

struct Expression *MakeIdentifier(struct Arena *arena, Symbol id)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_IDENTIFIER;
    result->identifier_id = id;
    return result;
}

struct Expression *MakeApply(struct Arena *arena, struct Expression *func_expr,
                             struct Expression *arg_expr)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_APPLY;
    result->apply_function = func_expr;
    result->apply_argument = arg_expr;
    return result;
}

struct Expression *MakeLet(struct Arena *arena, Symbol variable,
                           struct Expression *value, struct Expression *body)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_LET;
    result->let_id = variable;
    result->let_value = value;
    result->let_body = body;
    return result;
}

struct Expression *MakeLetRec(struct Arena *arena, Symbol variable,
                              struct Expression *value, struct Expression *body)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_LETREC;
    result->let_id = variable;
    result->let_value = value;
    result->let_body = body;
    return result;
}

struct Expression *MakeIf(struct Arena *arena, struct Expression *test,
                          struct Expression *then_branch,
                          struct Expression *else_branch)
{
    struct Expression *then_else;
    then_else = ArenaAllocate(arena, sizeof(struct Expression));
    then_else->type = EXP_THEN_ELSE;
    then_else->then_then = then_branch;
    then_else->then_else = else_branch;

    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_IF;
    result->if_test = test;
    result->if_then_else = then_else;
    return result;
}

struct Expression *MakeBinary(struct Arena *arena, MILLIE_TOKEN op,
                              struct Expression *left,
                              struct Expression *right)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_BINARY;
    result->binary_operator = op;
    result->binary_left = left;
    result->binary_right = right;
    return result;
}

struct Expression *MakeUnary(struct Arena *arena, MILLIE_TOKEN op,
                             struct Expression *arg)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_UNARY;
    result->unary_operator = op;
    result->unary_arg = arg;
    return result;
}

struct Expression *MakeBooleanLiteral(struct Arena *arena, bool value)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = value ? EXP_TRUE : EXP_FALSE;
    return result;
}

struct Expression *MakeIntegerLiteral(struct Arena *arena, uint64_t value)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_INTEGER_CONSTANT;
    result->literal_value = value;
    return result;
}

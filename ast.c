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

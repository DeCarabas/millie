#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

struct Expression *MakeLambda(struct Arena *arena, Symbol variable,
                              struct Expression *body)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_LAMBDA;
    result->id = variable;
    result->lambda_body = body;
    return result;
}

struct Expression *MakeIdentifier(struct Arena *arena, Symbol id)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_IDENTIFIER;
    result->id = id;
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
    result->id = variable;
    result->let_value = value;
    result->let_body = body;
    return result;
}

struct Expression *MakeLetRec(struct Arena *arena, Symbol variable,
                              struct Expression *value, struct Expression *body)
{
    struct Expression *result = ArenaAllocate(arena, sizeof(struct Expression));
    result->type = EXP_LETREC;
    result->id = variable;
    result->let_value = value;
    result->let_body = body;
    return result;
}

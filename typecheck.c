#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

// TODOTODO

/* Drive all of this with the printing function */
/* Might also be worthwhile to start on the test infrastructure */
/* TODO: Assign integers for type variable "names" at the end of checking. */
/*       They can be bytes; it can be an error to have more than 256 generic type variables. */
/* TODO: Environments can be easier */

/*
 * Type Variables
 */
typedef enum {
    TYPEEXP_INVALID = 0,
    TYPEEXP_ERROR,
    TYPEEXP_VARIABLE,
    TYPEEXP_GENERIC_VARIABLE,
    TYPEEXP_FUNC,
    TYPEEXP_INT,
    TYPEEXP_BOOL,
} TypeExpType;

struct TypeExp {
    TypeExpType type;
    union
    {
        struct TypeExp *arg_first;
        struct TypeExp *func_from;
        struct TypeExp *var_instance;
    };
    union
    {
        struct TypeExp *arg_second;
        struct TypeExp *func_to;
        struct TypeExp *var_temp_other;
    };
};

static struct TypeExp *_MakeTypeVar(struct Arena *arena)
{
    struct TypeExp *result = ArenaAllocate(arena, sizeof(struct TypeExp));
    result->type = TYPEEXP_VARIABLE;
    return result;
}

struct NonGenericTypeList {
    struct NonGenericTypeList *next;
    struct TypeExp *type;
};

struct NonGenericTypeList *
ExtendNonGenericTypeList(struct Arena *arena, struct TypeExp *type,
                         struct NonGenericTypeList *non_generics)
{
    struct NonGenericTypeList *new_list;
    new_list = ArenaAllocate(arena, sizeof(struct NonGenericTypeList));
    new_list->next = non_generics;
    new_list->type = type;
    return new_list;
}

static bool _IsTypeContainedWithin(struct TypeExp *a, struct TypeExp *b)
{
    if (a == b) { return true; }
    if (b == NULL) { return false; }
    if (_IsTypeContainedWithin(a, b->arg_first)) { return true; }
    if (_IsTypeContainedWithin(a, b->arg_second)) { return true; }
    return false;
}

static bool _IsTypeNonGeneric(struct TypeExp *a,
                              struct NonGenericTypeList *non_generics)
{
    while(non_generics) {
        if (_IsTypeContainedWithin(a, non_generics->type)) {
            return true;
        }
        non_generics = non_generics->next;
    }
    return false;
}

struct TypeExp *
PruneTypeExp(struct TypeExp *type)
{
    if (!type) { return NULL; }
    while(type->type == TYPEEXP_VARIABLE && type->var_instance) {
        type = type->var_instance;
    }
    return type;
}

struct TypeExp *
MakeFunctionTypeExp(struct Arena *arena, struct TypeExp *from_type,
                    struct TypeExp *to_type)
{
    struct TypeExp *result;
    result = ArenaAllocate(arena, sizeof(struct TypeExp));
    result->type = TYPEEXP_FUNC;
    result->func_from = from_type;
    result->func_to = to_type;
    return result;
}

static void _CleanupTypeVariables(struct TypeExp *type)
{
    type = PruneTypeExp(type);
    if (type == NULL) { return; }
    if (type->type == TYPEEXP_VARIABLE ||
        type->type == TYPEEXP_GENERIC_VARIABLE) {
        type->var_temp_other = NULL;
    } else {
        _CleanupTypeVariables(type->arg_first);
        _CleanupTypeVariables(type->arg_second);
    }
}

/*
 * _MakeTypeExpGenericImpl makes the given type expression generic by explicitly
 * replacing all free variables with generic variables. We can only do this for
 * variables that will no longer ever be unified, and those variables are *not*
 * contained within the type expressions in non_generics.
 *
 * The inverse of this function is `_MakeFreshTypeExp`.
 */
static struct TypeExp *_MakeGenericTypeExpImpl(
    struct Arena *arena,
    struct TypeExp *type,
    struct NonGenericTypeList *non_generics
)
{
    type = PruneTypeExp(type);
    switch(type->type) {
    case TYPEEXP_VARIABLE:
        {
            // If I've already visited this, don't check again.
            if (type->var_temp_other) {
                return type->var_temp_other;
            }

            // If this type is non generic in this scope then we don't modify it.
            if (_IsTypeNonGeneric(type, non_generics)) {
                type->var_temp_other = type;
                return type;
            }

            // This is a free type variable, it can be generic.
            struct TypeExp *result;
            result = ArenaAllocate(arena, sizeof(struct TypeExp));
            result->type = TYPEEXP_GENERIC_VARIABLE;
            type->var_temp_other = result;
            return result;
        }
        break;
    case TYPEEXP_FUNC:
        {
            struct TypeExp *arg_first = _MakeGenericTypeExpImpl(
                arena,
                type->arg_first,
                non_generics
            );
            struct TypeExp *arg_second = _MakeGenericTypeExpImpl(
                arena,
                type->arg_second,
                non_generics
            );
            if ((arg_first == type->arg_first) &&
                (arg_second == type->arg_second))
            {
                // Nothing in this was generic at all.
                return type;
            }

            struct TypeExp *result = ArenaAllocate(arena, sizeof(struct TypeExp));
            result->type = type->type;
            result->arg_first = arg_first;
            result->arg_second = arg_second;
            return result;
        }
    case TYPEEXP_INVALID:
    case TYPEEXP_INT:
    case TYPEEXP_BOOL:
    case TYPEEXP_ERROR:
    case TYPEEXP_GENERIC_VARIABLE:
        break;
    }

    return type;
}

static struct TypeExp *_MakeGenericTypeExp(
    struct Arena *arena,
    struct TypeExp *type,
    struct NonGenericTypeList *non_generics
)
{
    struct TypeExp *result = _MakeGenericTypeExpImpl(arena, type, non_generics);
    _CleanupTypeVariables(type);
    return result;
}

/*
 * _MakeFreshTypeExp converts a type expression that contains generic variables
 * into one that does not contain generic variables, that is,
 * `TYPEEXP_GENERIC_VARIABLE` becomes `TYPEEXP_VARIABLE`.
 */
static struct TypeExp *_MakeFreshTypeExpCopy(struct Arena *arena,
                                             struct TypeExp *type)
{
    type = PruneTypeExp(type);
    switch(type->type) {
    case TYPEEXP_GENERIC_VARIABLE:
        {
            if (type->var_temp_other) {
                return type->var_temp_other;
            }

            type->var_temp_other = _MakeTypeVar(arena);
            return type->var_temp_other;
        }
        break;
    case TYPEEXP_FUNC:
        {
            struct TypeExp *arg_first, *arg_second;
            arg_first = _MakeFreshTypeExpCopy(arena, type->arg_first);
            arg_second = _MakeFreshTypeExpCopy(arena, type->arg_second);

            if (arg_first == type->arg_first &&
                arg_second == type->arg_second) {
                return type;
            }

            struct TypeExp *result = ArenaAllocate(arena, sizeof(struct TypeExp));
            result->type = type->type;
            result->arg_first = arg_first;
            result->arg_second = arg_second;
            return result;
        }
        break;
    case TYPEEXP_VARIABLE:
    case TYPEEXP_BOOL:
    case TYPEEXP_INT:
    case TYPEEXP_ERROR:
    case TYPEEXP_INVALID:
        break;
    }
    return type;
}

struct TypeExp *_MakeFreshTypeExp(struct Arena *arena, struct TypeExp *type)
{
    struct TypeExp *fresh_type;
    fresh_type = _MakeFreshTypeExpCopy(arena, type);
    _CleanupTypeVariables(fresh_type);
    return fresh_type;
}

static struct TypeExp IntegerTypeExp = { TYPEEXP_INT,   {NULL}, {NULL}};
static struct TypeExp BooleanTypeExp = { TYPEEXP_BOOL,  {NULL}, {NULL}};
static struct TypeExp ErrorTypeExp   = { TYPEEXP_ERROR, {NULL}, {NULL}};

bool _IsErrorType(struct TypeExp *type) {
    return type == &ErrorTypeExp || type->type == TYPEEXP_ERROR;
}

/*
 * Type Environments
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct TypeEnvironment {
    struct TypeEnvironment *parent;
    struct TypeExp *type;
    Symbol id;
};

#pragma GCC diagnostic pop

struct TypeEnvironment *BindType(struct Arena *arena,
                                 struct TypeEnvironment *parent,
                                 Symbol id,
                                 struct TypeExp *type);

struct TypeEnvironment *BindType(struct Arena *arena, struct TypeEnvironment *parent, Symbol id,
         struct TypeExp *type)
{
    struct TypeEnvironment *result;
    result = ArenaAllocate(arena, sizeof(struct TypeEnvironment));
    result->parent = parent;
    result->id = id;
    result->type = type;
    return result;
}

static struct TypeExp *_LookupType(struct Arena *arena,
                                   struct TypeEnvironment *env,
                                   Symbol id)
{
    while(env != NULL) {
        if (env->id == id) {
            return _MakeFreshTypeExp(arena, env->type);
        }
        env = env->parent;
    }
    return NULL;
}

/*
 * Type checking/inference
 */
struct CheckContext {
    struct Arena *arena;
    struct Errors **errors;
    struct MillieTokens *tokens;
};

void Unify(struct CheckContext *context, struct Expression *node,
           struct TypeExp *type_one, struct TypeExp *type_two);
struct TypeExp *Analyze(struct CheckContext *context, struct Expression *node,
                        struct TypeEnvironment *env,
                        struct NonGenericTypeList *non_generics);

static void _ReportTypeError(struct CheckContext *context,
                             struct Expression *node,
                             const char *error)
{
    struct MillieToken start_token = GetToken(context->tokens, node->start_token);
    struct MillieToken end_token = GetToken(context->tokens, node->end_token);

    AddErrorF(
        context->errors,
        start_token.start,
        end_token.start + end_token.length,
        "Type Error: %s",
        error
    );
}

void Unify(struct CheckContext *context, struct Expression *node,
           struct TypeExp *type_one, struct TypeExp *type_two)
{
    type_one = PruneTypeExp(type_one);
    type_two = PruneTypeExp(type_two);

    if (_IsErrorType(type_one) || _IsErrorType(type_two)) {
        return;
    }

    // If there's only one `TYPEEXP_VARIABLE` then put it in type_one.
    // (If there's two it doesn't matter.)
    if (type_two->type == TYPEEXP_VARIABLE) {
        struct TypeExp *tmp = type_two;
        type_two = type_one;
        type_one = tmp;
    }

    if (type_one->type == TYPEEXP_VARIABLE) {
        if (type_one == type_two) { return; }
        if (_IsTypeContainedWithin(type_one, type_two)) {
            _ReportTypeError(context, node, "Self-recursive type");
        } else {
            type_one->var_instance = type_two;
        }
    } else {
        if (type_one->type != type_two->type) {
            _ReportTypeError(context, node, "Mismatched types");
            return;
        }

        if (type_one->arg_first) {
            Unify(context, node, type_one->arg_first, type_two->arg_first);
        }
        if (type_one->arg_second) {
            Unify(context, node, type_one->arg_second, type_two->arg_second);
        }
    }
}

struct TypeExp *
Analyze(struct CheckContext *context, struct Expression *node,
        struct TypeEnvironment *env, struct NonGenericTypeList *non_generics)
{
    switch(node->type) {
    case EXP_IDENTIFIER:
        {
            struct TypeExp *type = _LookupType(
                context->arena,
                env,
                node->identifier_id
            );
            if (type == NULL) {
                _ReportTypeError(context, node, "Unbound identifier");
                return &ErrorTypeExp;
            }
            return type;
        }
    case EXP_APPLY:
        {
            struct TypeExp *function_type = Analyze(
                context,
                node->apply_function,
                env,
                non_generics
            );
            struct TypeExp *arg_type = Analyze(
                context,
                node->apply_argument,
                env,
                non_generics
            );
            if (_IsErrorType(function_type) || _IsErrorType(arg_type)) {
                return &ErrorTypeExp;
            }
            struct TypeExp *result_type = _MakeTypeVar(context->arena);
            Unify(
                context,
                node,
                MakeFunctionTypeExp(
                    context->arena,
                    arg_type,
                    result_type
                ),
                function_type);
            return result_type;
        }
    case EXP_LAMBDA:
        {
            struct TypeExp *arg_type = _MakeTypeVar(context->arena);
            struct TypeEnvironment *new_env = BindType(
                context->arena,
                env,
                node->lambda_id,
                arg_type
            );
            struct NonGenericTypeList *new_non_generic = ExtendNonGenericTypeList(
                context->arena,
                arg_type,
                non_generics
            );
            struct TypeExp *result_type = Analyze(
                context,
                node->lambda_body,
                new_env,
                new_non_generic
            );
            return MakeFunctionTypeExp(context->arena, arg_type, result_type);
        }
    case EXP_LET:
        {
            struct TypeExp *defn_type = Analyze(
                context,
                node->let_value,
                env,
                non_generics
            );
            defn_type = _MakeGenericTypeExp(
                context->arena,
                defn_type,
                non_generics
            );

            struct TypeEnvironment *new_env = BindType(
                context->arena,
                env,
                node->let_id,
                defn_type
            );
            return Analyze(context, node->let_body, new_env, non_generics);
        }
    case EXP_LETREC:
        {
            struct TypeExp *new_type = _MakeTypeVar(context->arena);

            struct TypeEnvironment *new_env = BindType(
                context->arena,
                env,
                node->let_id,
                new_type
            );
            struct NonGenericTypeList *new_non_generic = ExtendNonGenericTypeList(
                context->arena,
                new_type,
                non_generics
            );
            struct TypeExp *defn_type = Analyze(
                context,
                node->let_value,
                new_env,
                new_non_generic
            );
            Unify(context, node, new_type, defn_type);

            // Rebind the new type variable to the generic version of the type
            // so I don't have to re-do new_env.
            new_type->var_instance = _MakeGenericTypeExp(
                context->arena,
                new_type,
                non_generics
            );

            return Analyze(context, node->let_body, new_env, non_generics);
        }
    case EXP_IF:
        {
            struct TypeExp *cond_type, *then_type, *else_type;
            cond_type = Analyze(context, node->if_test, env, non_generics);
            Unify(context, node->if_test, cond_type, &BooleanTypeExp);
            then_type = Analyze(context, node->if_then, env, non_generics);
            else_type = Analyze(context, node->if_else, env, non_generics);
            Unify(context, node, then_type, else_type);
            return then_type;
        }
    case EXP_BINARY:
        {
            // OK, dumb stuff, because this lets you add functions.
            struct TypeExp *left, *right;
            left = Analyze(context, node->binary_left, env, non_generics);
            right = Analyze(context, node->binary_right, env, non_generics);
            Unify(context, node, left, right);
            if (node->binary_operator == TOK_EQUALS) {
                return &BooleanTypeExp;
            }

            return left;
        }
    case EXP_UNARY:
        {
            // This lets you negate functions?
            struct TypeExp *arg;
            arg = Analyze(context, node->unary_arg, env, non_generics);
            return arg;
        }
    case EXP_INTEGER_CONSTANT:
        return &IntegerTypeExp;
    case EXP_TRUE:
    case EXP_FALSE:
        return &BooleanTypeExp;
    case EXP_INVALID:
    case EXP_ERROR:
        _ReportTypeError(context, node, "Invalid expression structure");
        break;
    }
    return &ErrorTypeExp;
}

struct TypeExp *TypeExpression(
    struct Arena *arena,
    struct MillieTokens *tokens,
    struct Expression *node,
    struct Errors **errors)
{
    struct CheckContext context;
    context.arena = arena;
    context.tokens = tokens;
    context.errors = errors;

    return Analyze(&context, node, NULL, NULL);
}

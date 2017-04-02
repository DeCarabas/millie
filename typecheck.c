#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif


struct NonGenericTypeList {
    struct NonGenericTypeList *next;
    struct TypeExp *type;
};

static struct NonGenericTypeList * _ExtendNonGenericTypeList(
    struct Arena *arena,
    struct TypeExp *type,
    struct NonGenericTypeList *non_generics
)
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

static struct TypeExp *_PruneTypeExp(struct TypeExp *type)
{
    if (!type) { return NULL; }
    while(type->type == TYPEEXP_VARIABLE && type->var_instance) {
        type = type->var_instance;
    }
    return type;
}

static struct TypeExp *_MakeFunctionType(struct Arena *arena,
                                         struct TypeExp *from_type,
                                         struct TypeExp *to_type)
{
    struct TypeExp *result;
    result = ArenaAllocate(arena, sizeof(struct TypeExp));
    result->type = TYPEEXP_FUNC;
    result->func_from = from_type;
    result->func_to = to_type;
    return result;
}

static struct TypeExp *_MakeTypeVar(struct Arena *arena)
{
    struct TypeExp *result = ArenaAllocate(arena, sizeof(struct TypeExp));
    result->type = TYPEEXP_VARIABLE;
    return result;
}

static void _CleanupTypeVariables(struct TypeExp *type)
{
    type = _PruneTypeExp(type);
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
    type = _PruneTypeExp(type);
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
    type = _PruneTypeExp(type);
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
    _CleanupTypeVariables(type);
    return fresh_type;
}

static struct TypeExp _IntegerTypeExp = { TYPEEXP_INT,   {NULL}, {NULL}};
static struct TypeExp _BooleanTypeExp = { TYPEEXP_BOOL,  {NULL}, {NULL}};
static struct TypeExp _ErrorTypeExp   = { TYPEEXP_ERROR, {NULL}, {NULL}};

bool _IsErrorType(struct TypeExp *type) {
    return type == &_ErrorTypeExp || type->type == TYPEEXP_ERROR;
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

static struct TypeEnvironment *_BindType(struct Arena *arena,
                                         struct TypeEnvironment *parent,
                                         Symbol id,
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
 * Formatting
 */
static const char *type_names[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R"
};

struct MString *_FormatTypeExpressionImpl(struct TypeExp *type, int *counter)
{
    type = _PruneTypeExp(type);
    switch(type->type) {
    case TYPEEXP_ERROR:
        return MStringCreate("{{Error}}");

    case TYPEEXP_VARIABLE:
    case TYPEEXP_GENERIC_VARIABLE:
        {
            struct MString *name = (struct MString *)type->var_temp_other;
            if (!name) {
                name = MStringPrintF("'%s", type_names[*counter]);
                type->var_temp_other = (struct TypeExp *)name;
                (*counter)++;
            }
            return MStringCopy(name);
        }
        break;

    case TYPEEXP_INT:
        return MStringCreate("int");

    case TYPEEXP_BOOL:
        return MStringCreate("bool");

    case TYPEEXP_FUNC:
        {
            struct MString *from, *to, *result;
            from = _FormatTypeExpressionImpl(type->func_from, counter);
            to = _FormatTypeExpressionImpl(type->func_to, counter);
            result = MStringPrintF("( %s -> %s )", MStringData(from), MStringData(to));
            MStringFree(&from);
            MStringFree(&to);
            return result;
        }

    case TYPEEXP_INVALID:
        break;
    }

    return MStringCreate("{{Invalid}}");
}

struct MString *FormatTypeExpression(struct TypeExp *type)
{
    int counter = 0;
    struct MString *str = _FormatTypeExpressionImpl(type, &counter);
    _CleanupTypeVariables(type);
    return str;
}

/*
 * Type checking/inference
 */
struct CheckContext {
    struct Arena *arena;
    struct Errors **errors;
    struct MillieTokens *tokens;
};

static void _ReportTypeError(struct CheckContext *context,
                             struct Expression *node,
                             struct MString *error_message)
{
    struct MillieToken start_token = GetToken(context->tokens, node->start_token);
    struct MillieToken end_token = GetToken(context->tokens, node->end_token);

    AddError(
        context->errors,
        start_token.start,
        end_token.start + end_token.length,
        error_message
    );
}


typedef enum UnificationError {
    UNIFY_SELF_RECURSIVE,
    UNIFY_INVALID_FUNCTION_APPLY,
    UNIFY_INCONSITENT_RECURSION,
    UNIFY_IF_CONDITION_BOOLEAN,
    UNIFY_IF_BRANCHES_SAME,
    UNIFY_NO_VALID_BINARY_OPERATOR,
} UnificationError;

struct UnifyContext {
    struct CheckContext *context;
    struct Expression *expression;
    UnificationError error_code;
    struct TypeExp *original_one;
    struct TypeExp *original_two;
};

static void _ReportUnificationFailure(struct UnifyContext *unify_context)
{
    struct MStringStatic st;
    struct MString *error_message = NULL;
    switch(unify_context->error_code) {
    case UNIFY_SELF_RECURSIVE:
        {
            struct MString *arg_one, *arg_two;
            arg_one = FormatTypeExpression(unify_context->original_one);
            arg_two = FormatTypeExpression(unify_context->original_two);
            error_message = MStringPrintF(
                "unsupported recursive type: the type \"%s\" is contained "
                "within the type \"%s\"",
                MStringData(arg_one),
                MStringData(arg_two)
            );
        }
        break;
    case UNIFY_INVALID_FUNCTION_APPLY:
        {
            struct MString *arg_one, *arg_two;
            arg_one = FormatTypeExpression(unify_context->original_one);
            arg_two = FormatTypeExpression(unify_context->original_two);
            error_message = MStringPrintF(
                "the function of type \"%s\" cannot be used as a function of "
                "type \"%s\"; either the argument or return type is "
                "incompatible",
                MStringData(arg_two),
                MStringData(arg_one)
            );
        }
        break;
    case UNIFY_INCONSITENT_RECURSION:
        {
            struct MString *arg_one, *arg_two;
            arg_one = FormatTypeExpression(unify_context->original_one);
            arg_two = FormatTypeExpression(unify_context->original_two);
            error_message = MStringPrintF(
                "inconsistent recursive definition: unable to reconcile the "
                "two necessary types \"%s\" and \"%s\"",
                MStringData(arg_one),
                MStringData(arg_two)
            );
        }
        break;
    case UNIFY_IF_CONDITION_BOOLEAN:
        {
            struct MString *arg_one;
            arg_one = FormatTypeExpression(unify_context->original_one);
            error_message = MStringPrintF(
                "condition of an if expression must be a boolean (not \"%s\")",
                MStringData(arg_one)
            );
        }
        break;
    case UNIFY_IF_BRANCHES_SAME:
        {
            struct MString *arg_one, *arg_two;
            arg_one = FormatTypeExpression(unify_context->original_one);
            arg_two = FormatTypeExpression(unify_context->original_two);
            error_message = MStringPrintF(
                "then branch returns \"%s\" and else branch returns \"%s\"; "
                "both branches of the condition must have the same type",
                MStringData(arg_one),
                MStringData(arg_two)
            );
        }
        break;
    case UNIFY_NO_VALID_BINARY_OPERATOR:
        {
            struct MString *arg_one, *arg_two;
            arg_one = FormatTypeExpression(unify_context->original_one);
            arg_two = FormatTypeExpression(unify_context->original_two);
            error_message = MStringPrintF(
                "no operator takes types \"%s\" and \"%s\"",
                MStringData(arg_one),
                MStringData(arg_two)
            );
        }
        break;
    }
    if (!error_message) {
        error_message = MStringCreateStatic("unification failure", &st);
    }

    _ReportTypeError(
        unify_context->context,
        unify_context->expression,
        error_message
    );
}

static void _UnifyImpl(struct UnifyContext *context, struct TypeExp *type_one,
                       struct TypeExp *type_two)
{
    type_one = _PruneTypeExp(type_one);
    type_two = _PruneTypeExp(type_two);

    // Bail early if we've already detected some kind of type error here.
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
            context->error_code = UNIFY_SELF_RECURSIVE;
            _ReportUnificationFailure(context);
        } else {
            type_one->var_instance = type_two;
        }
    } else {
        if (type_one->type != type_two->type) {
            _ReportUnificationFailure(context);
            return;
        }

        if (type_one->arg_first) {
            _UnifyImpl(
                context,
                type_one->arg_first,
                type_two->arg_first
            );
        }
        if (type_one->arg_second) {
            _UnifyImpl(
                context,
                type_one->arg_second,
                type_two->arg_second
            );
        }
    }
}

static void _Unify(struct CheckContext *context, struct Expression *node,
                   UnificationError error_code, struct TypeExp *type_one,
                   struct TypeExp *type_two)
{
    struct UnifyContext unify_context;
    unify_context.context = context;
    unify_context.expression = node;
    unify_context.error_code = error_code;
    unify_context.original_one = type_one;
    unify_context.original_two = type_two;

    return _UnifyImpl(&unify_context, type_one, type_two);
}


static struct TypeExp *_Analyze(struct CheckContext *context,
                                struct Expression *node,
                                struct TypeEnvironment *env,
                                struct NonGenericTypeList *non_generics);

static struct TypeExp *_AnalyzeApply(struct CheckContext *context,
                                struct Expression *node,
                                struct TypeEnvironment *env,
                                struct NonGenericTypeList *non_generics)
{
    struct TypeExp *function_type = _Analyze(
        context,
        node->apply_function,
        env,
        non_generics
    );
    struct TypeExp *arg_type = _Analyze(
        context,
        node->apply_argument,
        env,
        non_generics
    );
    if (_IsErrorType(function_type) || _IsErrorType(arg_type)) {
        return &_ErrorTypeExp;
    }
    struct TypeExp *result_type = _MakeTypeVar(context->arena);
    _Unify(
        context,
        node,
        UNIFY_INVALID_FUNCTION_APPLY,
        _MakeFunctionType(
            context->arena,
            arg_type,
            result_type
        ),
        function_type);
    return result_type;
}

static struct TypeExp *_AnalyzeIdentifier(
    struct CheckContext *context,
    struct Expression *node,
    struct TypeEnvironment *env
)
{
    struct TypeExp *type = _LookupType(
        context->arena,
        env,
        node->identifier_id
    );
    if (type == NULL) {
        struct MStringStatic st;
        _ReportTypeError(
            context,
            node,
            MStringCreateStatic("Unbound identifier", &st)
        );
        return &_ErrorTypeExp;
    }
    return type;
}

static struct TypeExp *_AnalyzeLambda(
    struct CheckContext *context,
    struct Expression *node,
    struct TypeEnvironment *env,
    struct NonGenericTypeList *non_generics
)
{
    struct TypeExp *arg_type = _MakeTypeVar(context->arena);
    struct TypeEnvironment *new_env = _BindType(
        context->arena,
        env,
        node->lambda_id,
        arg_type
    );
    struct NonGenericTypeList *new_non_generic;
    new_non_generic = _ExtendNonGenericTypeList(
        context->arena,
        arg_type,
        non_generics
    );
    struct TypeExp *result_type = _Analyze(
        context,
        node->lambda_body,
        new_env,
        new_non_generic
    );
    return _MakeFunctionType(context->arena, arg_type, result_type);
}

static struct TypeExp *_AnalyzeLet(
    struct CheckContext *context,
    struct Expression *node,
    struct TypeEnvironment *env,
    struct NonGenericTypeList *non_generics
)
{
    struct TypeExp *defn_type = _Analyze(
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

    struct TypeEnvironment *new_env = _BindType(
        context->arena,
        env,
        node->let_id,
        defn_type
    );
    return _Analyze(context, node->let_body, new_env, non_generics);
}

static struct TypeExp *_AnalyzeLetRec(
    struct CheckContext *context,
    struct Expression *node,
    struct TypeEnvironment *env,
    struct NonGenericTypeList *non_generics
)
{
    struct TypeExp *new_type = _MakeTypeVar(context->arena);

    struct TypeEnvironment *new_env = _BindType(
        context->arena,
        env,
        node->let_id,
        new_type
    );
    struct NonGenericTypeList *new_non_generic;
    new_non_generic = _ExtendNonGenericTypeList(
        context->arena,
        new_type,
        non_generics
    );
    struct TypeExp *defn_type = _Analyze(
        context,
        node->let_value,
        new_env,
        new_non_generic
    );
    _Unify(context, node, UNIFY_INCONSITENT_RECURSION, new_type, defn_type);

    // Rebind the new type variable to the generic version of the type so I
    // don't have to re-do new_env.
    new_type->var_instance = _MakeGenericTypeExp(
        context->arena,
        new_type,
        non_generics
    );

    return _Analyze(context, node->let_body, new_env, non_generics);
}

static struct TypeExp *_AnalyzeIf(
    struct CheckContext *context,
    struct Expression *node,
    struct TypeEnvironment *env,
    struct NonGenericTypeList *non_generics
)
{
    struct TypeExp *cond_type, *then_type, *else_type;
    cond_type = _Analyze(context, node->if_test, env, non_generics);
    _Unify(
        context,
        node->if_test,
        UNIFY_IF_CONDITION_BOOLEAN,
        cond_type,
        &_BooleanTypeExp
    );

    then_type = _Analyze(context, node->if_then, env, non_generics);
    else_type = _Analyze(context, node->if_else, env, non_generics);
    _Unify(context, node, UNIFY_IF_BRANCHES_SAME, then_type, else_type);
    return then_type;
}

struct OperatorEntry {
    MILLIE_TOKEN token;
    struct TypeExp *left;
    struct TypeExp *right;
    struct TypeExp *result;
};

struct OperatorEntry _operators[] = {
    { TOK_PLUS,   &_IntegerTypeExp, &_IntegerTypeExp, &_IntegerTypeExp },
    { TOK_MINUS,  &_IntegerTypeExp, &_IntegerTypeExp, &_IntegerTypeExp },
    { TOK_STAR,   &_IntegerTypeExp, &_IntegerTypeExp, &_IntegerTypeExp },
    { TOK_SLASH,  &_IntegerTypeExp, &_IntegerTypeExp, &_IntegerTypeExp },

    { TOK_EQUALS, NULL,             NULL,             &_BooleanTypeExp },

    { TOK_EOF, 0, 0, 0 },
};

static struct TypeExp *_AnalyzeBinary(
    struct CheckContext *context,
    struct Expression *node,
    struct TypeEnvironment *env,
    struct NonGenericTypeList *non_generics
)
{
    // OK, dumb stuff, because this lets you add functions.
    struct TypeExp *left, *right;
    left = _Analyze(context, node->binary_left, env, non_generics);
    right = _Analyze(context, node->binary_right, env, non_generics);

    struct OperatorEntry *op = _operators;
    while(op->token != TOK_EOF) {
        if (op->token == node->binary_operator) {
            if (op->left) {
                _Unify(
                    context,
                    node,
                    UNIFY_NO_VALID_BINARY_OPERATOR,
                    left,
                    op->left
                );
            }
            if (op->right) {
                _Unify(
                    context,
                    node,
                    UNIFY_NO_VALID_BINARY_OPERATOR,
                    right,
                    op->right
                );
            }

            _Unify(context, node, UNIFY_NO_VALID_BINARY_OPERATOR, left, right);
            return op->result;
        }
        op++;
    }

    Fail("Binary operator not found in type table");
    return &_ErrorTypeExp;
}

static struct TypeExp *_AnalyzeUnary(
    struct CheckContext *context,
    struct Expression *node,
    struct TypeEnvironment *env,
    struct NonGenericTypeList *non_generics
)
{
    // This lets you negate functions?
    struct TypeExp *arg;
    arg = _Analyze(context, node->unary_arg, env, non_generics);
    return arg;
}

static struct TypeExp *_Analyze(struct CheckContext *context,
                                struct Expression *node,
                                struct TypeEnvironment *env,
                                struct NonGenericTypeList *non_generics)
{
    switch(node->type) {
    case EXP_IDENTIFIER: return _AnalyzeIdentifier(context, node, env);
    case EXP_APPLY: return _AnalyzeApply(context, node, env, non_generics);
    case EXP_LAMBDA: return _AnalyzeLambda(context, node, env, non_generics);
    case EXP_LET: return _AnalyzeLet(context, node, env, non_generics);
    case EXP_LETREC: return _AnalyzeLetRec(context, node, env, non_generics);
    case EXP_IF: return _AnalyzeIf(context, node, env, non_generics);
    case EXP_BINARY: return _AnalyzeBinary(context, node, env, non_generics);
    case EXP_UNARY: return _AnalyzeUnary(context, node, env, non_generics);
    case EXP_INTEGER_CONSTANT: return &_IntegerTypeExp;

    case EXP_TRUE:
    case EXP_FALSE:
        return &_BooleanTypeExp;

    case EXP_INVALID:
    case EXP_ERROR:
        {
            struct MStringStatic st;
            _ReportTypeError(
                context,
                node,
                MStringCreateStatic("Invalid expression structure", &st)
            );
        }
        break;
    }
    return &_ErrorTypeExp;
}

struct TypeExp *GetExpressionType(
    struct Arena *arena,
    struct MillieTokens *tokens,
    struct Expression *node,
    struct Errors **errors)
{
    struct CheckContext context;
    context.arena = arena;
    context.tokens = tokens;
    context.errors = errors;

    return _Analyze(&context, node, NULL, NULL);
}

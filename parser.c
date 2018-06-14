#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

// struct ParseContext is the record of things we're tracking while parsing, so
// we don't have to pass quite so many arguments.
struct ParseContext {
    struct Arena *arena;
    struct MString *buffer;
    struct MillieToken *tokens;
    struct SymbolTable *table;
    struct Errors **errors;
    uint32_t pos;
    uint32_t lost_count;
};

// ----------------------------------------------------------------------------
// Helper functions for parsing
// ----------------------------------------------------------------------------

static MILLIE_TOKEN _PeekToken(struct ParseContext *context)
{
    return context->tokens[context->pos].type;
}

static struct MillieToken _PrevTokenStruct(struct ParseContext *context)
{
    return context->tokens[context->pos-1];
}

static MILLIE_TOKEN _PrevToken(struct ParseContext *context)
{
    return context->tokens[context->pos-1].type;
}

static uint32_t _PrevPos(struct ParseContext *context)
{
    return context->pos-1;
}

static bool _IsAtEnd(struct ParseContext *context)
{
    return _PeekToken(context) == TOK_EOF;
}

static MILLIE_TOKEN _Advance(struct ParseContext *context)
{
    if (!_IsAtEnd(context)) { context->pos += 1; }
    return _PrevToken(context);
}

static bool _Check(struct ParseContext *context, MILLIE_TOKEN token)
{
    if (_IsAtEnd(context)) { return false; }
    return _PeekToken(context) == token;
}

static void _SyntaxErrorToken(struct ParseContext *context,
                              struct MillieToken token, const char *message)
{
    const uint32_t RESYNC_TOKENS = 4;
    if (0 == context->lost_count)
    {
        unsigned int error_start = token.start;
        unsigned int error_end = error_start + token.length;

        struct MString *em = MStringCreate(message);
        AddError(context->errors, error_start, error_end, em);
        MStringFree(&em);
    }
    context->lost_count = RESYNC_TOKENS;
}

static void _SyntaxError(struct ParseContext *context, const char *message)
{
    struct MillieToken token = context->tokens[context->pos];
    _SyntaxErrorToken(context, token, message);
}

static bool _MatchV(struct ParseContext *context, int count, ...)
{
    bool matched = false;
    va_list ap;
    va_start(ap, count);
    for(int i = 0; i < count; i++) {
        MILLIE_TOKEN token = va_arg(ap, MILLIE_TOKEN);
        if (_Check(context, token)) {
            _Advance(context);
            matched = true;
            break;
        }
    }

    va_end(ap);
    return matched;
}

static bool _Match(struct ParseContext *context, MILLIE_TOKEN token)
{
    if (_Check(context, token)) {
        if (context->lost_count) {
            // Every time we find something that makes sense we get a little
            // more confident...
            context->lost_count -= 1;
        }
        _Advance(context);
        return true;
    }

    return false;
}

static void _Expect(struct ParseContext *context, MILLIE_TOKEN token,
                    const char *error)
{
    if (!_Match(context, token)) {
        _SyntaxError(context, error);
    }
}

static Symbol _ParseSymbol(struct ParseContext *context)
{
    Symbol symbol = INVALID_SYMBOL;
    if (_Match(context, TOK_ID)) {
        // TODO: Work out how to avoid the allocation here without screwing up
        //       and writing to the source buffer. (The catch is null
        //       termination.)
        struct MillieToken id_token = _PrevTokenStruct(context);
        struct MString *token_value = MStringCreateN(
            MStringData(context->buffer) + id_token.start,
            id_token.length
        );
        symbol = FindOrCreateSymbol(context->table, token_value);
        MStringFree(&token_value);
    } else {
        _SyntaxError(context, "Expected an identifier");
    }
    return symbol;
}

// ----------------------------------------------------------------------------
// Productions
// ----------------------------------------------------------------------------

static struct Expression *_ParseExpr(struct ParseContext *context);

static struct Expression *_ParseTuple(struct ParseContext *context);

static struct Expression *_ParseTupleNext(struct ParseContext *context,
                                          struct Expression *first)
{
    struct Expression *rest = _ParseTuple(context);

    int length = 2; // Assume that we just parsed the end.
    if (rest->type == EXP_TUPLE) {
        length = rest->tuple_length + 1;
    }

    return MakeTuple(context->arena, first, rest, length);
}

static struct Expression *_ParseTuple(struct ParseContext *context)
{
    struct Expression *car = _ParseExpr(context);
    if (_Match(context, TOK_COMMA)) {
        return _ParseTupleNext(context, car);
    } else {
        return MakeTupleFinal(context->arena, car);
    }
}

static struct Expression *_ParsePrimary(struct ParseContext *context)
{
    if (_Match(context, TOK_FALSE)) {
        return MakeBooleanLiteral(context->arena, _PrevPos(context), false);
    }
    if (_Match(context, TOK_TRUE)) {
        return MakeBooleanLiteral(context->arena, _PrevPos(context), true);
    }
    if (_Match(context, TOK_INT_LITERAL)) {
        uint64_t value = 0;
        struct MillieToken token = _PrevTokenStruct(context);
        const char *str = MStringData(context->buffer) + token.start;
        for(unsigned int i = 0; i < token.length; i++, str++) {
            uint64_t new_value = (value * 10) + (uint64_t)(*str - '0');
            if (new_value < value) {
                _SyntaxErrorToken(context, token, "Integer literal overflow");
                break;
            }
            value = new_value;
        }
        return MakeIntegerLiteral(context->arena,  _PrevPos(context), value);
    }
    if (_Check(context, TOK_ID)) {
        Symbol sym = _ParseSymbol(context);
        return MakeIdentifier(context->arena, _PrevPos(context), sym);
    }
    if (_Match(context, TOK_LPAREN)) {
        struct Expression *expr = _ParseExpr(context);
        if (_Match(context, TOK_COMMA)) {
            expr = _ParseTupleNext(context, expr);
        }
        _Expect(context, TOK_RPAREN, "Expected a ')' after the expression.");
        return expr;
    }

    _SyntaxError(context, "Expected an expression.");
    return MakeSyntaxError(context->arena, context->pos);
}

static struct Expression *_ParseApplication(struct ParseContext *context)
{
    struct Expression *expr = _ParsePrimary(context);

    while (_PeekToken(context) >= TOK_FIRST_PRIMARY &&
           _PeekToken(context) <= TOK_LAST_PRIMARY) {

        struct Expression *arg = _ParsePrimary(context);
        expr = MakeApply(context->arena, expr, arg);
    }

    return expr;
}

static struct Expression *_ParseUnary(struct ParseContext *context)
{
    if (_MatchV(context, 2, TOK_PLUS, TOK_MINUS)) {
        MILLIE_TOKEN operator = _PrevToken(context);
        uint32_t token_pos = _PrevPos(context);
        struct Expression *right = _ParseUnary(context);
        return MakeUnary(context->arena, token_pos, operator, right);
    }

    return _ParseApplication(context);
}

static struct Expression *_ParseFactor(struct ParseContext *context)
{
    struct Expression *expr = _ParseUnary(context);

    while(_MatchV(context, 2, TOK_STAR, TOK_SLASH)) {
        MILLIE_TOKEN operator = _PrevToken(context);
        struct Expression *right = _ParseUnary(context);
        expr = MakeBinary(context->arena, operator, expr, right);
    }

    return expr;
}

static struct Expression *_ParseTerm(struct ParseContext *context)
{
    struct Expression *expr = _ParseFactor(context);

    while(_MatchV(context, 2, TOK_PLUS, TOK_MINUS)) {
        MILLIE_TOKEN operator = _PrevToken(context);
        struct Expression *right = _ParseFactor(context);
        expr = MakeBinary(context->arena, operator, expr, right);
    }

    return expr;
}

static struct Expression *_ParseComparison(struct ParseContext *context)
{
    struct Expression *expr = _ParseTerm(context);

    while(_Match(context, TOK_EQUALS)) {
        MILLIE_TOKEN operator = _PrevToken(context);
        struct Expression *right = _ParseTerm(context);
        expr = MakeBinary(context->arena, operator, expr, right);
    }

    return expr;
}

static struct Expression *_ParseFn(struct ParseContext *context)
{
    if (_Match(context, TOK_FN)) {
        uint32_t token_pos = _PrevPos(context);
        Symbol variable = _ParseSymbol(context);
        _Expect(
            context,
            TOK_ARROW,
            "Expected an => between variable and function body."
        );
        struct Expression *body = _ParseExpr(context);

        return MakeLambda(context->arena, token_pos, variable, body);
    }

    return _ParseComparison(context);
}

static struct Expression *_ParseIf(struct ParseContext *context)
{
    if (_Match(context, TOK_IF)) {
        uint32_t token_pos = _PrevPos(context);
        struct Expression *test = _ParseExpr(context);
        _Expect(context, TOK_THEN, "Expected 'then' after the condition.");
        struct Expression *then_arm = _ParseExpr(context);
        _Expect(context, TOK_ELSE, "Expected 'else' after the 'then' arm.");
        struct Expression *else_arm = _ParseExpr(context);

        return MakeIf(context->arena, token_pos, test, then_arm, else_arm);
    }

    return _ParseFn(context);
}

static struct Expression *_ParseLet(struct ParseContext *context)
{
    if (_Match(context, TOK_LET)) {
        uint32_t token_pos = _PrevPos(context);

        bool is_let_rec = false;
        if (_Match(context, TOK_REC)) {
            is_let_rec = true;
        }

        Symbol variable = _ParseSymbol(context);
        _Expect(
            context,
            TOK_EQUALS,
            "Expected an '=' after the variable in the let."
        );
        struct Expression *value = _ParseExpr(context);
        _Expect(
            context,
            TOK_IN,
            "Expected an 'in' after the variable value in the let."
        );
        struct Expression *body = _ParseExpr(context);

        if (is_let_rec) {
            return MakeLetRec(context->arena, token_pos, variable, value, body);
        } else {
            return MakeLet(context->arena, token_pos, variable, value, body);
        }
    }

    return _ParseIf(context);
}

static struct Expression *_ParseExpr(struct ParseContext *context)
{
    return _ParseLet(context);
}

struct Expression *ParseExpression(struct Arena *arena,
                                   struct MillieTokens *tokens,
                                   struct SymbolTable *symbol_table,
                                   struct Errors **errors)
{
    struct ParseContext context;
    context.arena = arena;
    context.buffer = tokens->buffer;
    context.tokens = tokens->token_array->buffer;
    context.pos = 0;
    context.table = symbol_table;
    context.errors = errors;
    context.lost_count = 0;

    return _ParseExpr(&context);
}

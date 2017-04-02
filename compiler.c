#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

struct CompileContext {
    uint8_t *code;
    uint8_t *code_write;
    int code_capacity;

    uint8_t integer_registers;

    struct Errors **errors;
    struct MillieTokens *tokens;
};

typedef enum MILLIE_OPCODE {
    OP_HALT,
    OP_LOADI_8,
    OP_LOADI_16,
    OP_LOADI_32,
    OP_LOADI_64,
} MILLIE_OPCODE;

static void _ReportCompileError(struct CompileContext *context,
                                struct Expression *expression,
                                const char *error)
{
    struct MillieToken start_token = GetToken(
        context->tokens,
        expression->start_token
    );
    struct MillieToken end_token = GetToken(
        context->tokens,
        expression->end_token
    );

    struct MStringStatic st;
    struct MString *error_string = MStringCreateStatic(error, &st);
    AddError(
        context->errors,
        start_token.start,
        end_token.start + end_token.length,
        error_string);
}

static void _EnsureCodeCapacity(struct CompileContext *context, int more)
{
    int code_len = context->code_write - context->code;
    if (context->code_capacity - code_len < more) {
        uint32_t new_capacity = context->code_capacity * 2;
        context->code = realloc(context->code, new_capacity);
        context->code_capacity = new_capacity;
    }
}

static void _WriteInstructionU8(struct CompileContext *context, uint8_t value)
{
    _EnsureCodeCapacity(context, 1);
    *(context->code_write++) = value;
}

static void _WriteInstructionU16(struct CompileContext *context, uint16_t value)
{
    _EnsureCodeCapacity(context, 4);
    *(context->code_write++) = (value & 0x0000FF) >> 0;
    *(context->code_write++) = (value & 0x00FF00) >> 8;
}

static void _WriteInstructionU32(struct CompileContext *context, uint32_t value)
{
    _EnsureCodeCapacity(context, 4);
    *(context->code_write++) = (value & 0x000000FF) >> 0;
    *(context->code_write++) = (value & 0x0000FF00) >> 8;
    *(context->code_write++) = (value & 0x00FF0000) >> 16;
    *(context->code_write++) = (value & 0xFF000000) >> 24;
}

static void _WriteInstructionU64(struct CompileContext *context, uint64_t value)
{
    _EnsureCodeCapacity(context, 8);
    *(context->code_write++) = (value & 0x00000000000000FF) >> 0;
    *(context->code_write++) = (value & 0x000000000000FF00) >> 8;
    *(context->code_write++) = (value & 0x0000000000FF0000) >> 16;
    *(context->code_write++) = (value & 0x00000000FF000000) >> 24;
    *(context->code_write++) = (value & 0x000000FF00000000) >> 32;
    *(context->code_write++) = (value & 0x0000FF0000000000) >> 40;
    *(context->code_write++) = (value & 0x00FF000000000000) >> 48;
    *(context->code_write++) = (value & 0xFF00000000000000) >> 56;
}

static uint8_t _GetFreeIntRegister(struct CompileContext *context)
{
    context->integer_registers++;
    return context->integer_registers - 1;
}

static uint8_t _CompileExpression(struct CompileContext *context,
                                   struct Expression *expression);

static uint8_t _CompileIntegerLiteral(struct CompileContext *context,
                                      struct Expression *expression)
{
    uint8_t reg = _GetFreeIntRegister(context);
    if (expression->literal_value <= UINT8_MAX) {
        _WriteInstructionU8(context, OP_LOADI_8);
        _WriteInstructionU8(context, reg);
        _WriteInstructionU8(context, expression->literal_value);
    } else if (expression->literal_value <= UINT16_MAX) {
        _WriteInstructionU8(context, OP_LOADI_16);
        _WriteInstructionU8(context, reg);
        _WriteInstructionU16(context, expression->literal_value);
    } else if (expression->literal_value <= UINT32_MAX) {
        _WriteInstructionU8(context, OP_LOADI_32);
        _WriteInstructionU8(context, reg);
        _WriteInstructionU32(context, expression->literal_value);
    } else {
        _WriteInstructionU8(context, OP_LOADI_64);
        _WriteInstructionU8(context, reg);
        _WriteInstructionU64(context, expression->literal_value);
    }
    return reg;
}

static uint8_t _CompileExpression(struct CompileContext *context,
                                  struct Expression *expression)
{
    switch(expression->type) {
    case EXP_INTEGER_CONSTANT: return _CompileIntegerLiteral(context, expression);
    case EXP_ERROR: break;
    case EXP_LAMBDA:
    case EXP_IDENTIFIER:
    case EXP_APPLY:
    case EXP_LET:
    case EXP_LETREC:
    case EXP_TRUE:
    case EXP_FALSE:
    case EXP_IF:
    case EXP_BINARY:
    case EXP_UNARY:
    case EXP_INVALID:
        _ReportCompileError(
            context,
            expression,
            "Unsupported compile expression"
        );
        break;
    }
    return 0;
}

struct CompiledExpression {
    uint8_t *code;
    size_t code_length;
    uint8_t result_register;
};

#define INITIAL_CODE_CAPACITY (64)

void CompileExpression(struct Expression *expression,
                       struct MillieTokens *tokens,
                       struct Errors **errors,
                       struct CompiledExpression *result)
{
    struct CompileContext context;
    context.code = malloc(INITIAL_CODE_CAPACITY);
    context.code_write = context.code;
    context.code_capacity = INITIAL_CODE_CAPACITY;

    context.integer_registers = 0;
    context.errors = errors;
    context.tokens = tokens;

    result->result_register = _CompileExpression(&context, expression);
    result->code = context.code;
    result->code_length = context.code_write - context.code;
}

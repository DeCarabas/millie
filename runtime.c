#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

struct Frame {
    uint64_t *registers;
};

static uint8_t _ReadU8(const uint8_t **buffer_ptr);
static uint16_t _ReadU16(const uint8_t **buffer_ptr);
static uint32_t _ReadU32(const uint8_t **buffer_ptr);
static uint64_t _ReadU64(const uint8_t **buffer_ptr);

// For watching the execution of the VM, obvs.
// #define VM_TRACE

#ifdef VM_TRACE

typedef enum OP_ARG_TYPE {
    OPARG_0 = 0,
    OPARG_REG,
    OPARG_DREG,
    OPARG_OFF,
    OPARG_U8,
    OPARG_U16,
    OPARG_U32,
    OPARG_U64,
    OPARG_IDX,
} OP_ARG_TYPE;

static const struct OpInfo {
    const char *name;
    OP_ARG_TYPE args[3];
} _op_info[] = {
#define Q(x) #x
#define OPCODE(name, arg0, arg1, arg2) {                        \
        Q(name) ,                                               \
        {                                                       \
            OPARG_##arg0 ,                                      \
            OPARG_##arg1 ,                                      \
            OPARG_##arg2 ,                                      \
        }                                                       \
    },
#include "opcodes.inc"
#undef OPCODE
#undef Q
};

// TraceInstruction disassembles an instruction and prints the details.
static const uint8_t *_TraceInstruction(const uint8_t *ip,
                                        struct CompiledExpression *def)
{
    const struct OpInfo *info = &(_op_info[*ip]);
    ptrdiff_t offset = ip - def->code;
    fprintf(stderr, "%05td %s", offset, info->name);
    ip++;
    for(int i = 0; i < 3; i++) {
        switch(info->args[i]) {
        case OPARG_REG: fprintf(stderr, " r%d",     _ReadU8(&ip)); break;
        case OPARG_DREG: fprintf(stderr, " => r%d", _ReadU8(&ip)); break;
        case OPARG_U8:  fprintf(stderr, " %02x",    _ReadU8(&ip)); break;
        case OPARG_U16: fprintf(stderr, " %04x",    _ReadU16(&ip)); break;
        case OPARG_U32: fprintf(stderr, " %08x",    _ReadU32(&ip)); break;
        case OPARG_U64: fprintf(stderr, " %llx",    _ReadU64(&ip)); break;
        case OPARG_IDX: fprintf(stderr, "[%d]",  (int16_t)_ReadU16(&ip)); break;
        case OPARG_OFF:
            {
                int16_t offset = (int16_t)_ReadU16(&ip);
                ptrdiff_t target_offset = (ip + offset) - def->code;
                fprintf(stderr, " %d (%05td)", offset, target_offset);
            }
            break;
        default: break;
        }
    }
    return ip;
}

static void _TraceStep(const uint8_t *code,
                     struct CompiledExpression *def,
                     struct Frame *frame)
{
    fprintf(stderr, "  Frame: %p\n", frame);
    for(size_t i = 0; i < def->register_count; i++) {
        fprintf(stderr, "    r%zu: 0x%llx\n", i, frame->registers[i]);
    }
    _TraceInstruction(code, def);
    fprintf(stderr, "\n\n");
}

static void _TraceFunctionBody(struct Module *module,
                               int func_id,
                               const uint8_t *curr_ip,
                               uint64_t closure,
                               uint64_t arg0)
{
    const char *what = (curr_ip == NULL) ? "Called" : "Returned to";
    fprintf(stderr, "%s %p: %d\n", what, module, func_id);
    fprintf(stderr, "  Closure: %llx  Arg0: %llx\n", closure, arg0);

    struct CompiledExpression *def = &(module->functions[func_id]);
    const uint8_t *ip = def->code;

    if (curr_ip == NULL) { curr_ip = ip; }

    while((size_t)(ip - def->code) < def->code_length) {
        if (ip == curr_ip) {
            fprintf(stderr, " > ");
        } else {
            fprintf(stderr, "   ");
        }
        ip = _TraceInstruction(ip, def);
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "  Output in register r%d\n", def->result_register);
    fprintf(stderr, "\n");
}

#define TRACE_STEP(ip, def, frame) _TraceStep(ip, def, frame)
#define TRACE_ENTER(module, func_id, closure, arg0) \
    _TraceFunctionBody(module, func_id, NULL, closure, arg0)
#define TRACE_RETURN(module, func_id, ip, closure, arg0) \
    _TraceFunctionBody(module, func_id, ip, closure, arg0)

#else

#define TRACE_STEP(ip, def, frame)
#define TRACE_ENTER(module, func_id, closure, arg0)
#define TRACE_RETURN(module, func_id, ip, closure, arg0)

#endif



static uint8_t _ReadU8(const uint8_t **buffer_ptr) {
    const uint8_t *buffer = *buffer_ptr;
    *buffer_ptr += 1;
    return buffer[0];
}

static uint16_t _ReadU16(const uint8_t **buffer_ptr) {
    const uint8_t *buffer = *buffer_ptr;
    *buffer_ptr += 2;
    return
        (((uint16_t)buffer[0]) << 0) |
        (((uint16_t)buffer[1]) << 8);
}

static uint32_t _ReadU32(const uint8_t **buffer_ptr) {
    const uint8_t *buffer = *buffer_ptr;
    *buffer_ptr += 4;
    return
        (((uint32_t)buffer[0]) << 0)  |
        (((uint32_t)buffer[1]) << 8)  |
        (((uint32_t)buffer[2]) << 16) |
        (((uint32_t)buffer[3]) << 24);
}

static uint64_t _ReadU64(const uint8_t **buffer_ptr) {
    const uint8_t *buffer = *buffer_ptr;
    *buffer_ptr += 8;
    return
        (((uint64_t)buffer[0]) << 0)  |
        (((uint64_t)buffer[1]) << 8)  |
        (((uint64_t)buffer[2]) << 16) |
        (((uint64_t)buffer[3]) << 24) |
        (((uint64_t)buffer[4]) << 32) |
        (((uint64_t)buffer[5]) << 40) |
        (((uint64_t)buffer[6]) << 48) |
        (((uint64_t)buffer[7]) << 56);
}

size_t LifetimeAllocations = 0;

static struct RuntimeClosure *_AllocateClosure(int func_id, int slot_count)
{
    size_t alloc_size =
        sizeof(struct RuntimeClosure) + (slot_count * sizeof(uint64_t));

    LifetimeAllocations += alloc_size;
    struct RuntimeClosure *closure = malloc(alloc_size);
    closure->function_id = func_id;

    return closure;
}

static uint64_t *_AllocateTuple(uint64_t size)
{
    size_t alloc_size = size * sizeof(uint64_t);

    LifetimeAllocations += alloc_size;
    uint64_t *tuple = malloc(alloc_size);

    return tuple;
}

uint64_t EvaluateCode(struct Module *module,
                      int func_id,
                      uint64_t closure,
                      uint64_t arg0)
{
    TRACE_ENTER(module, func_id, closure, arg0);

    struct CompiledExpression *code = &(module->functions[func_id]);
    struct Frame frame;
    frame.registers = calloc(code->register_count, sizeof(uint64_t));
    frame.registers[0] = closure;
    frame.registers[1] = arg0;

    const uint8_t *ip = code->code;
    bool halt = false;
    while(!halt) {
        TRACE_STEP(ip, code, &frame);
        MILLIE_OPCODE op = *(ip++);
        switch(op) {

        case OP_LOADI_8:
            {
                uint8_t val = _ReadU8(&ip);
                uint8_t reg = _ReadU8(&ip);
                frame.registers[reg] = val;
            }
            break;
        case OP_LOADI_16:
            {
                uint16_t val = _ReadU16(&ip);
                uint8_t reg = _ReadU8(&ip);
                frame.registers[reg] = val;
            }
            break;
        case OP_LOADI_32:
            {
                uint32_t val = _ReadU32(&ip);
                uint8_t reg = _ReadU8(&ip);
                frame.registers[reg] = val;
            }
            break;
        case OP_LOADI_64:
            {
                uint64_t val = _ReadU64(&ip);
                uint8_t reg = _ReadU8(&ip);
                frame.registers[reg] = val;
            }
            break;

        case OP_RET:
            halt = true;
            break;

        case OP_CALL:
            {
                uint8_t func_reg = _ReadU8(&ip);
                uint8_t arg_reg = _ReadU8(&ip);
                uint8_t ret_reg = _ReadU8(&ip);

                uint64_t closure = frame.registers[func_reg];
                int function_id = (int)((uint64_t *)closure)[0];

                uint64_t arg_val = frame.registers[arg_reg];
                uint64_t retval = EvaluateCode(
                    module,
                    function_id,
                    closure,
                    arg_val
                );
                frame.registers[ret_reg] = retval;

                TRACE_RETURN(module, func_id, ip, closure, arg0);
            }
            break;

        case OP_ADD:
            {
                uint8_t left_reg = _ReadU8(&ip);
                uint8_t right_reg = _ReadU8(&ip);
                uint8_t ret_reg = _ReadU8(&ip);

                frame.registers[ret_reg] =
                    frame.registers[left_reg] + frame.registers[right_reg];
            }
            break;

        case OP_SUB:
            {
                uint8_t left_reg = _ReadU8(&ip);
                uint8_t right_reg = _ReadU8(&ip);
                uint8_t ret_reg = _ReadU8(&ip);

                frame.registers[ret_reg] =
                    frame.registers[left_reg] - frame.registers[right_reg];
            }
            break;

        case OP_MUL:
            {
                uint8_t left_reg = _ReadU8(&ip);
                uint8_t right_reg = _ReadU8(&ip);
                uint8_t ret_reg = _ReadU8(&ip);

                frame.registers[ret_reg] =
                    frame.registers[left_reg] * frame.registers[right_reg];
            }
            break;

        case OP_NEG:
            {
                uint8_t arg_reg = _ReadU8(&ip);
                uint8_t ret_reg = _ReadU8(&ip);

                frame.registers[ret_reg] = -frame.registers[arg_reg];
            }
            break;

        case OP_EQ:
            {
                uint8_t left_reg = _ReadU8(&ip);
                uint8_t right_reg = _ReadU8(&ip);
                uint8_t ret_reg = _ReadU8(&ip);

                if (frame.registers[left_reg] == frame.registers[right_reg]) {
                    frame.registers[ret_reg] = 1;
                } else {
                    frame.registers[ret_reg] = 0;
                }
            }
            break;

        case OP_JZ:
            {
                uint8_t test_reg = _ReadU8(&ip);
                int16_t offset = (int16_t)_ReadU16(&ip);

                if (frame.registers[test_reg] == 0) {
                    ip += offset;
                }
            }
            break;

        case OP_JMP:
            {
                int16_t offset = (int16_t)_ReadU16(&ip);
                ip += offset;
            }
            break;

        case OP_MOV:
            {
                uint8_t src_reg = _ReadU8(&ip);
                uint8_t dst_reg = _ReadU8(&ip);

                frame.registers[dst_reg] = frame.registers[src_reg];
            }
            break;

        case OP_NEW_CLOSURE:
            {
                uint8_t funcid_reg = _ReadU8(&ip);
                uint8_t dst_reg = _ReadU8(&ip);

                int func_id = (int)frame.registers[funcid_reg];
                struct CompiledExpression *target;
                target = &(module->functions[func_id]);

                struct RuntimeClosure *closure;
                if (target->closure_length > 0) {
                    closure = _AllocateClosure(func_id, target->closure_length);
                } else {
                    closure = &(target->static_closure);
                }

                frame.registers[dst_reg] = (uint64_t)closure;
            }
            break;

        case OP_LOADA_64:
            {
                uint8_t src_reg = _ReadU8(&ip);
                int16_t offset = (int16_t)_ReadU16(&ip);
                uint8_t dst_reg = _ReadU8(&ip);

                uint64_t *arr = (uint64_t *)(frame.registers[src_reg]);
                uint64_t result = arr[offset];
                frame.registers[dst_reg] = result;
            }
            break;

        case OP_STOREA_64:
            {
                uint8_t src_reg = _ReadU8(&ip);
                int16_t offset = (int16_t)_ReadU16(&ip);
                uint8_t val_reg = _ReadU8(&ip);

                uint64_t *arr = (uint64_t *)(frame.registers[src_reg]);
                arr[offset] = frame.registers[val_reg];
            }
            break;

        case OP_NEW_TUPLE:
            {
                uint8_t len_reg = _ReadU8(&ip);
                uint8_t dst_reg = _ReadU8(&ip);

                uint64_t *tuple = _AllocateTuple(frame.registers[len_reg]);
                frame.registers[dst_reg] = (uint64_t)tuple;
            }
            break;

        default:
            fprintf(stderr, "ERROR: UNKNOWN INSTRUCTION: %d\n", op);
            halt = true;
            break;
        }
    }

    uint64_t result = frame.registers[code->result_register];
    free(frame.registers);
    return result;
}

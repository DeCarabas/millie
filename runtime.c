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

static void _TraceOp(const uint8_t *code,
                     struct CompiledExpression *def,
                     struct Frame *frame)
{
    const struct OpInfo *info = &(_op_info[*code]);
    ptrdiff_t offset = code - def->code;
    {
        fprintf(stderr, "%05td %s", offset, info->name);
        code++;
        for(int i = 0; i < 3; i++) {
            switch(info->args[i]) {
            case OPARG_REG: fprintf(stderr, " r%d",     _ReadU8(&code)); break;
            case OPARG_DREG: fprintf(stderr, " => r%d", _ReadU8(&code)); break;
            case OPARG_OFF: fprintf(stderr, " %d",      _ReadU16(&code)); break;
            case OPARG_U8:  fprintf(stderr, " %02x",    _ReadU8(&code)); break;
            case OPARG_U16: fprintf(stderr, " %04x",    _ReadU16(&code)); break;
            case OPARG_U32: fprintf(stderr, " %08x",    _ReadU32(&code)); break;
            case OPARG_U64: fprintf(stderr, " %016llx", _ReadU64(&code)); break;
            default: break;
            }
        }
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "  Frame: %p\n", frame);
    for(size_t i = 0; i < def->register_count; i++) {
        fprintf(stderr, "    r%zu: 0x%llx\n", i, frame->registers[i]);
    }
}

#define TRACE_VM(ip, def, frame) _TraceOp(ip, def, frame)

#else

#define TRACE_VM(ip, def, frame)

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

uint64_t EvaluateCode(struct Module *module, int func_id, uint64_t arg0) {
    struct CompiledExpression *code = &(module->functions[func_id]);
    struct Frame frame;
    frame.registers = calloc(code->register_count, sizeof(uint64_t));
    frame.registers[0] = arg0;

    const uint8_t *ip = code->code;
    bool halt = false;
    while(!halt) {
        TRACE_VM(ip, code, &frame);
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

        case OP_HALT:
            halt = true;
            break;

        case OP_CALL:
            {
                uint8_t func_reg = _ReadU8(&ip);
                uint8_t arg_reg = _ReadU8(&ip);
                uint8_t ret_reg = _ReadU8(&ip);

                int function_id = (int)frame.registers[func_reg];
                uint64_t arg_val = frame.registers[arg_reg];
                uint64_t retval = EvaluateCode(module, function_id, arg_val);
                frame.registers[ret_reg] = retval;
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

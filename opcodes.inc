// Opcodes for the millie bytecode VM.
// These are expected to be included in somewhere that defines OPCODE.
// The OPCODE macro is defined as:
//
//    OPCODE(symbol, arg0, arg1, arg2)
//
// The <arg> bits are currently just to inform the disassembler; nothing else
// uses them, although they should obviously reflect what the compiler writes
// and what the VM reads. The various arg types are:
//
//   U8, U16, U32, U64: Unsigned constant values.
//   REG:               A register to read from.
//   DREG:              A destination register.
//   OFFS:              A 16-bit signed offset.
//
// Note that all destination registers *may* re-use source registers; the
// machine is guaranteed to read all source registers before writing to the
// destination registers.
//
// == The instructions ==
//
// HALT returns from the current frame.
OPCODE(HALT, 0, 0, 0)

// LOADI loads an immediate out of the instruction stream into a register.
OPCODE(LOADI_8,  U8,  DREG, 0)
OPCODE(LOADI_16, U16, DREG, 0)
OPCODE(LOADI_32, U32, DREG, 0)
OPCODE(LOADI_64, U64, DREG, 0)

// CALL calls a function, indicated by the function pointer in the first
// register. The single argument value is in the second register, and the
// third register has the destination for the resulting value.
OPCODE(CALL, REG, REG, DREG)

// ADD, SUB, and MUL are simple arithmetic from register to register.
OPCODE(ADD, REG, REG, DREG)
OPCODE(SUB, REG, REG, DREG)
OPCODE(MUL, REG, REG, DREG)

// NEG is unary negation.
OPCODE(NEG, REG, DREG, 0)

// EQ compares the two registers for equality and writes a 1 or a 0 in the
// destination register.
OPCODE(EQ, REG, REG, DREG)

// JMP jumps to another location. The location is specified as an offset from
// the end of the entire instruction. For example, if the 'JMP' byte is at
// offset 7 and the OFF argument is 5, then the destination will be offset 15:
// 7 (the starting point) + 5 (the offset) + 1 (for the opcode) + 2 (for the
// offset).
OPCODE(JMP, OFF,  0,   0)

// JZ jumps to another location if the specified register contains zero. The
// jump target is computed as it is for JMP, above.
OPCODE(JZ,  REG,  OFF, 0)

// MOV copies the contents of one register to another register.
OPCODE(MOV, REG, DREG,  0)

// Opcodes for the millie bytecode VM.
// These are expected to be included in somewhere that defines OPCODE.
// The OPCODE macro is defined as:
//
//    OPCODE(symbol, arg0, arg1, arg2)
//
// The <arg> bits are currently just to inform the disassembler; nothing else
// uses them, although they should obviously reflect what the compiler writes
// and what the VM reads.
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

// Math
OPCODE(ADD, REG, REG, DREG)
OPCODE(SUB, REG, REG, DREG)
OPCODE(MUL, REG, REG, DREG)
OPCODE(NEG, REG, REG, DREG)

// Comparison
OPCODE(EQ, REG, REG, DREG)

// Jump
OPCODE(JZ,  REG,  OFF, 0)
OPCODE(JMP, OFF,  0,   0)

// Other
OPCODE(MOV, REG, DREG,  0)

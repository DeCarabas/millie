// HALT returns from the current frame.
OPCODE(HALT, 0, 0, 0)

// LOADI loads an immediate out of the instruction stream into a register.
OPCODE(LOADI_8,  REG, U8,  0)
OPCODE(LOADI_16, REG, U16, 0)
OPCODE(LOADI_32, REG, U32, 0)
OPCODE(LOADI_64, REG, U64, 0)

// CALL calls a function
OPCODE(CALL, REG, REG, REG)

// Math
OPCODE(ADD, REG, REG, REG)
OPCODE(SUB, REG, REG, REG)
OPCODE(MUL, REG, REG, REG)
OPCODE(NEG, REG, REG, REG)

// Comparison
OPCODE(EQ, REG, REG, REG)

// Jump
OPCODE(JZ,  REG,  OFF, 0)
OPCODE(JMP, OFF,  0,   0)

// Other
OPCODE(MOV, REG, REG,  0)


enum class ThumbDataOp {
  AND = 0,
  EOR = 1,
  LSL = 2,
  LSR = 3,
  ASR = 4,
  ADC = 5,
  SBC = 6,
  ROR = 7,
  TST = 8,
  NEG = 9,
  CMP = 10,
  CMN = 11,
  ORR = 12,
  MUL = 13,
  BIC = 14,
  MVN = 15
};

enum class ThumbHighRegOp {
  ADD = 0,
  CMP = 1,
  MOV = 2,
  BLX = 3
};

template <int op, int imm>
void Thumb_MoveShiftedRegister(u16 instruction) {
  int dst   = (instruction >> 0) & 7;
  int src   = (instruction >> 3) & 7;
  int carry = m_state.cpsr.c;

  u32 result = m_state.reg[src];

  DoShift(op, result, imm, carry, true);

  m_state.cpsr.c = carry;
  m_state.cpsr.z = (result == 0);
  m_state.cpsr.n = result >> 31;
  
  m_state.reg[dst] = result;
  m_state.r15 += 2;
}

template <bool immediate, bool subtract, int field3>
void Thumb_AddSub(u16 instruction) {
  int dst = (instruction >> 0) & 7;
  int src = (instruction >> 3) & 7;
  u32 operand = immediate ? field3 : m_state.reg[field3];

  if(subtract) {
    m_state.reg[dst] = SUB(m_state.reg[src], operand, true);
  } else {
    m_state.reg[dst] = ADD(m_state.reg[src], operand, true);
  }

  m_state.r15 += 2;
}

template <int op, int dst>
void Thumb_MoveCompareAddSubImm(u16 instruction) {
  u32 imm = instruction & 0xFF;

  switch(op) {
    case 0b00:
      // MOV
      m_state.reg[dst] = imm;
      m_state.cpsr.n = 0;
      m_state.cpsr.z = imm == 0;
      break;
    case 0b01:
      // CMP
      SUB(m_state.reg[dst], imm, true);
      break;
    case 0b10:
      // ADD
      m_state.reg[dst] = ADD(m_state.reg[dst], imm, true);
      break;
    case 0b11:
      // SUB
      m_state.reg[dst] = SUB(m_state.reg[dst], imm, true);
      break;
  }

  m_state.r15 += 2;
}

template <ThumbDataOp opcode>
void Thumb_ALU(u16 instruction) {
  int dst = (instruction >> 0) & 7;
  int src = (instruction >> 3) & 7;

  switch(opcode) {
    case ThumbDataOp::AND: {
      m_state.reg[dst] &= m_state.reg[src];
      SetZeroAndSignFlag(m_state.reg[dst]);
      break;
    }
    case ThumbDataOp::EOR: {
      m_state.reg[dst] ^= m_state.reg[src];
      SetZeroAndSignFlag(m_state.reg[dst]);
      break;
    }
    case ThumbDataOp::LSL: {
      int carry = m_state.cpsr.c;
      LSL(m_state.reg[dst], m_state.reg[src], carry);
      SetZeroAndSignFlag(m_state.reg[dst]);
      m_state.cpsr.c = carry;
      break;
    }
    case ThumbDataOp::LSR: {
      int carry = m_state.cpsr.c;
      LSR(m_state.reg[dst], m_state.reg[src], carry, false);
      SetZeroAndSignFlag(m_state.reg[dst]);
      m_state.cpsr.c = carry;
      break;
    }
    case ThumbDataOp::ASR: {
      int carry = m_state.cpsr.c;
      ASR(m_state.reg[dst], m_state.reg[src], carry, false);
      SetZeroAndSignFlag(m_state.reg[dst]);
      m_state.cpsr.c = carry;
      break;
    }
    case ThumbDataOp::ADC: {
      m_state.reg[dst] = ADC(m_state.reg[dst], m_state.reg[src], true);
      break;
    }
    case ThumbDataOp::SBC: {
      m_state.reg[dst] = SBC(m_state.reg[dst], m_state.reg[src], true);
      break;
    }
    case ThumbDataOp::ROR: {
      int carry = m_state.cpsr.c;
      ROR(m_state.reg[dst], m_state.reg[src], carry, false);
      SetZeroAndSignFlag(m_state.reg[dst]);
      m_state.cpsr.c = carry;
      break;
    }
    case ThumbDataOp::TST: {
      u32 result = m_state.reg[dst] & m_state.reg[src];
      SetZeroAndSignFlag(result);
      break;
    }
    case ThumbDataOp::NEG: {
      m_state.reg[dst] = SUB(0, m_state.reg[src], true);
      break;
    }
    case ThumbDataOp::CMP: {
      SUB(m_state.reg[dst], m_state.reg[src], true);
      break;
    }
    case ThumbDataOp::CMN: {
      ADD(m_state.reg[dst], m_state.reg[src], true);
      break;
    }
    case ThumbDataOp::ORR: {
      m_state.reg[dst] |= m_state.reg[src];
      SetZeroAndSignFlag(m_state.reg[dst]);
      break;
    }
    case ThumbDataOp::MUL: {
      m_state.reg[dst] *= m_state.reg[src];
      SetZeroAndSignFlag(m_state.reg[dst]);
      m_state.cpsr.c = 0;
      break;
    }
    case ThumbDataOp::BIC: {
      m_state.reg[dst] &= ~m_state.reg[src];
      SetZeroAndSignFlag(m_state.reg[dst]);
      break;
    }
    case ThumbDataOp::MVN: {
      m_state.reg[dst] = ~m_state.reg[src];
      SetZeroAndSignFlag(m_state.reg[dst]);
      break;
    }
  }

  m_state.r15 += 2;
}

template <ThumbHighRegOp opcode, bool high1, bool high2>
void Thumb_HighRegisterOps_BX(u16 instruction) {
  int dst = (instruction >> 0) & 7;
  int src = (instruction >> 3) & 7;

  if(high1) dst += 8;
  if(high2) src += 8;

  u32 operand = m_state.reg[src];

  if(src == 15) operand &= ~1;

  switch(opcode) {
    case ThumbHighRegOp::ADD: {
      m_state.reg[dst] += operand;
      if(dst == 15) {
        m_state.r15 &= ~1;
        ReloadPipeline16();
      } else {
        m_state.r15 += 2;
      }
      break;
    }
    case ThumbHighRegOp::CMP: {
      SUB(m_state.reg[dst], operand, true);
      m_state.r15 += 2;
      break;
    }
    case ThumbHighRegOp::MOV: {
      m_state.reg[dst] = operand;
      if(dst == 15) {
        m_state.r15 &= ~1;
        ReloadPipeline16();
      } else {
        m_state.r15 += 2;
      }
      break;
    }
    case ThumbHighRegOp::BLX: {
      // NOTE: "high1" is reused as link bit for branch exchange instructions.
      if(high1 && m_model != Model::ARM7) {
        m_state.r14 = (m_state.r15 - 2) | 1;
      }

      // LSB indicates new instruction set (0 = ARM, 1 = THUMB)
      if(operand & 1) {
        m_state.r15 = operand & ~1;
        ReloadPipeline16();
      } else {
        m_state.cpsr.thumb = 0;
        m_state.r15 = operand & ~3;
        ReloadPipeline32();
      }
      break;
    }
  }
}

template <int dst>
void Thumb_LoadStoreRelativePC(u16 instruction) {
  u32 offset  = instruction & 0xFF;
  u32 address = (m_state.r15 & ~2) + (offset << 2);

  m_state.reg[dst] = ReadWord(address);
  m_state.r15 += 2;
}

template <int op, int off>
void Thumb_LoadStoreOffsetReg(u16 instruction) {
  int dst  = (instruction >> 0) & 7;
  int base = (instruction >> 3) & 7;

  u32 address = m_state.reg[base] + m_state.reg[off];

  switch(op) {
    case 0b00:
      // STR rD, [rB, rO]
      WriteWord(address, m_state.reg[dst]);
      break;
    case 0b01:
      // STRB rD, [rB, rO]
      WriteByte(address, (u8)m_state.reg[dst]);
      break;
    case 0b10:
      // LDR rD, [rB, rO]
      m_state.reg[dst] = ReadWordRotate(address);
      break;
    case 0b11:
      // LDRB rD, [rB, rO]
      m_state.reg[dst] = ReadByte(address);
      break;
  }

  m_state.r15 += 2;
}

template <int op, int off>
void Thumb_LoadStoreSigned(u16 instruction) {
  int dst  = (instruction >> 0) & 7;
  int base = (instruction >> 3) & 7;

  u32 address = m_state.reg[base] + m_state.reg[off];

  switch(op) {
    case 0b00:
      // STRH rD, [rB, rO]
      WriteHalf(address, m_state.reg[dst]);
      break;
    case 0b01:
      // LDSB rD, [rB, rO]
      m_state.reg[dst] = ReadByteSigned(address);
      break;
    case 0b10:
      // LDRH rD, [rB, rO]
      m_state.reg[dst] = ReadHalfMaybeRotate(address);
      break;
    case 0b11:
      // LDSH rD, [rB, rO]
      m_state.reg[dst] = ReadHalfSigned(address);
      break;
  }

  m_state.r15 += 2;
}

template <int op, int imm>
void Thumb_LoadStoreOffsetImm(u16 instruction) {
  int dst  = (instruction >> 0) & 7;
  int base = (instruction >> 3) & 7;

  switch(op) {
    case 0b00:
      // STR rD, [rB, #imm]
      WriteWord(m_state.reg[base] + imm * 4, m_state.reg[dst]);
      break;
    case 0b01:
      // LDR rD, [rB, #imm]
      m_state.reg[dst] = ReadWordRotate(m_state.reg[base] + imm * 4);
      break;
    case 0b10:
      // STRB rD, [rB, #imm]
      WriteByte(m_state.reg[base] + imm, m_state.reg[dst]);
      break;
    case 0b11:
      // LDRB rD, [rB, #imm]
      m_state.reg[dst] = ReadByte(m_state.reg[base] + imm);
      break;
  }

  m_state.r15 += 2;
}

template <bool load, int imm>
void Thumb_LoadStoreHword(u16 instruction) {
  int dst  = (instruction >> 0) & 7;
  int base = (instruction >> 3) & 7;

  u32 address = m_state.reg[base] + imm * 2;

  if(load) {
    m_state.reg[dst] = ReadHalfMaybeRotate(address);
  } else {
    WriteHalf(address, m_state.reg[dst]);
  }

  m_state.r15 += 2;
}

template <bool load, int dst>
void Thumb_LoadStoreRelativeToSP(u16 instruction) {
  u32 offset  = instruction & 0xFF;
  u32 address = m_state.reg[13] + offset * 4;

  if(load) {
    m_state.reg[dst] = ReadWordRotate(address);
  } else {
    WriteWord(address, m_state.reg[dst]);
  }

  m_state.r15 += 2;
}

template <bool stackptr, int dst>
void Thumb_LoadAddress(u16 instruction) {
  u32 offset = (instruction  & 0xFF) << 2;

  if(stackptr) {
    m_state.reg[dst] = m_state.r13 + offset;
  } else {
    m_state.reg[dst] = (m_state.r15 & ~2) + offset;
  }

  m_state.r15 += 2;
}

template <bool sub>
void Thumb_AddOffsetToSP(u16 instruction) {
  u32 offset = (instruction  & 0x7F) * 4;

  m_state.r13 += sub ? -offset : offset;
  m_state.r15 += 2;
}

template <bool pop, bool rbit>
void Thumb_PushPop(u16 instruction) {
  u8  list = instruction & 0xFF;
  u32 address = m_state.r13;

  if(pop) {
    for(int reg = 0; reg <= 7; reg++) {
      if(list & (1 << reg)) {
        m_state.reg[reg] = ReadWord(address);
        address += 4;
      }
    }

    if(rbit) {
      m_state.reg[15] = ReadWord(address);
      m_state.reg[13] = address + 4;
      if((m_state.r15 & 1) || m_model == Model::ARM7) {
        m_state.r15 &= ~1;
        ReloadPipeline16();
      } else {
        m_state.cpsr.thumb = 0;
        ReloadPipeline32();
      }
      return;
    }

    m_state.r13 = address;
  } else {
    /* Calculate internal start address (final r13 value) */
    for(int reg = 0; reg <= 7; reg++) {
      if(list & (1 << reg))
        address -= 4;
    }

    if(rbit) {
      address -= 4;
    }

    /* Store address in r13 before we mess with it. */
    m_state.r13 = address;

    for(int reg = 0; reg <= 7; reg++) {
      if(list & (1 << reg)) {
        WriteWord(address, m_state.reg[reg]);
        address += 4;
      }
    }

    if(rbit) {
      WriteWord(address, m_state.r14);
    }
  }

  m_state.r15 += 2;
}

template <bool load, int base>
void Thumb_LoadStoreMultiple(u16 instruction) {
  u8  list = instruction & 0xFF;
  u32 address = m_state.reg[base];

  if(load) {
    for(int i = 0; i <= 7; i++) {
      if(list & (1 << i)) {
        m_state.reg[i] = ReadWord(address);
        address += 4;
      }
    }
    
    if(~list & (1 << base)) {
      m_state.reg[base] = address;
    }
  } else {
    for(int reg = 0; reg <= 7; reg++) {
      if(list & (1 << reg)) {
        WriteWord(address, m_state.reg[reg]);
        address += 4;
      }
    }

    m_state.reg[base] = address;
  }

  m_state.r15 += 2;
}

template <int cond>
void Thumb_ConditionalBranch(u16 instruction) {
  if(EvaluateCondition(static_cast<Condition>(cond))) {
    u32 imm = instruction & 0xFF;
    if(imm & 0x80) {
      imm |= 0xFFFFFF00;
    }

    m_state.r15 += imm * 2;
    ReloadPipeline16();
  } else {
    m_state.r15 += 2;
  }
}

void Thumb_SWI(u16 instruction) {
  (void)instruction;

  // Save current program status register.
  m_state.spsr[(int)Bank::Supervisor] = m_state.cpsr;

  // Enter SVC mode and disable IRQs.
  SwitchMode(Mode::Supervisor);
  m_state.cpsr.thumb = 0;
  m_state.cpsr.mask_irq = 1;

  // Save current program counter and jump to SVC exception vector.
  m_state.r14 = m_state.r15 - 2;
  m_state.r15 = m_exception_base + 0x08;
  ReloadPipeline32();
}

void Thumb_UnconditionalBranch(u16 instruction) {
  u32 imm = (instruction & 0x3FF) * 2;
  if(instruction & 0x400) {
    imm |= 0xFFFFF800;
  }

  m_state.r15 += imm;
  ReloadPipeline16();
}

void Thumb_LongBranchLinkPrefix(u16 instruction) {
  u32 imm = (instruction & 0x7FF) << 12;
  if(imm & 0x400000) {
    imm |= 0xFF800000;
  }

  m_state.r14 = m_state.r15 + imm;
  m_state.r15 += 2;
}

template <bool exchange>
void Thumb_LongBranchLinkSuffix(u16 instruction) {
  u32 imm  = instruction & 0x7FF;
  u32 temp = m_state.r15 - 2;

  // BLX does not exist in ARMv4T
  if(exchange && m_model == Model::ARM7) {
    Thumb_Undefined(instruction);
    return;
  }

  m_state.r15 = (m_state.r14 & ~1) + imm * 2;
  m_state.r14 = temp | 1;
  if(exchange) {
    m_state.r15 &= ~3;
    m_state.cpsr.thumb = 0;
    ReloadPipeline32();
  } else {
    ReloadPipeline16();
  }
}

void Thumb_Undefined(u16 instruction) {
  ATOM_PANIC("undefined Thumb instruction: 0x{:04X} (PC=0x{:08X})", instruction, m_state.r15);
}

void Thumb_Unimplemented(u16 instruction) {
  ATOM_PANIC("unimplemented Thumb instruction: 0x{:04X} (PC=0x{:08X})", instruction, m_state.r15);
}

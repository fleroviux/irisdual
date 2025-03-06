
void SetZeroAndSignFlag(u32 value) {
  m_state.cpsr.n = value >> 31;
  m_state.cpsr.z = (value == 0);
}

u32 ADD(u32 op1, u32 op2, bool set_flags) {
  if(set_flags) {
    u64 result64 = (u64)op1 + (u64)op2;
    u32 result32 = (u32)result64;

    SetZeroAndSignFlag(result32);
    m_state.cpsr.c = result64 >> 32;
    m_state.cpsr.v = (~(op1 ^ op2) & (op2 ^ result32)) >> 31;
    return result32;
  } else {
    return op1 + op2;
  }
}

u32 ADC(u32 op1, u32 op2, bool set_flags) {
  if(set_flags) {
    u64 result64 = (u64)op1 + (u64)op2 + (u64)m_state.cpsr.c;
    u32 result32 = (u32)result64;

    SetZeroAndSignFlag(result32);
    m_state.cpsr.c = result64 >> 32;
    m_state.cpsr.v = (~(op1 ^ op2) & (op2 ^ result32)) >> 31;
    return result32;
  } else {
    return op1 + op2 + m_state.cpsr.c;
  }
}

u32 SUB(u32 op1, u32 op2, bool set_flags) {
  u32 result = op1 - op2;

  if(set_flags) {
    SetZeroAndSignFlag(result);
    m_state.cpsr.c = op1 >= op2;
    m_state.cpsr.v = ((op1 ^ op2) & (op1 ^ result)) >> 31;
  }

  return result;
}

u32 SBC(u32 op1, u32 op2, bool set_flags) {
  u32 op3 = (m_state.cpsr.c) ^ 1;
  u32 result = op1 - op2 - op3;

  if(set_flags) {
    SetZeroAndSignFlag(result);
    m_state.cpsr.c = (u64)op1 >= (u64)op2 + (u64)op3;
    m_state.cpsr.v = ((op1 ^ op2) & (op1 ^ result)) >> 31;
  }

  return result;
}

u32 QADD(u32 op1, u32 op2, bool saturate = true) {
  u32 result = op1 + op2;

  if((~(op1 ^ op2) & (op2 ^ result)) >> 31) {
    m_state.cpsr.q = 1;
    if(saturate) {
      return 0x80000000 - (result >> 31);
    }
  }

  return result;
}

u32 QSUB(u32 op1, u32 op2) {
  u32 result = op1 - op2;

  if(((op1 ^ op2) & (op1 ^ result)) >> 31) {
    m_state.cpsr.q = 1;
    return 0x80000000 - (result >> 31);
  }

  return result;
}

void DoShift(int opcode, u32& operand, u8 amount, int& carry, bool immediate) {
  switch(opcode) {
    case 0: LSL(operand, amount, carry); break;
    case 1: LSR(operand, amount, carry, immediate); break;
    case 2: ASR(operand, amount, carry, immediate); break;
    case 3: ROR(operand, amount, carry, immediate); break;
  }
}

void LSL(u32& operand, u8 amount, int& carry) {
  const int adj_amount = std::min<int>(amount, 33);
  const u32 result = (u32)((u64)operand << adj_amount);
  if(adj_amount != 0) {
    carry = (u32)((u64)operand << (adj_amount - 1)) >> 31;
  }
  operand = result;
}

void LSR(u32& operand, u8 amount, int& carry, bool immediate) {
  // LSR #32 is encoded as LSR #0
  if(immediate && amount == 0) {
    amount = 32;
  }

  const int adj_amount = std::min<int>(amount, 33);
  const u32 result = (u32)((u64)operand >> adj_amount);
  if(adj_amount != 0) {
    carry = ((u64)operand >> (adj_amount - 1)) & 1u;
  }
  operand = result;
}

void ASR(u32& operand, u8 amount, int& carry, bool immediate) {
  // ASR #32 is encoded as ASR #0
  if(immediate && amount == 0) {
    amount = 32;
  }

  const int adj_amount = std::min<int>(amount, 33);
  const u32 result = (u32)((i64)(i32)operand >> adj_amount);
  if(adj_amount != 0) {
    carry = ((i64)(i32)operand >> (adj_amount - 1)) & 1u;
  }
  operand = result;
}

void ROR(u32& operand, u8 amount, int& carry, bool immediate) {
  // RRX #1 is encoded as ROR #0
  if(immediate && amount == 0) {
    const int lsb = operand & 1;
    operand = (operand >> 1) | (carry << 31);
    carry = lsb;
  } else {
    if(amount == 0) {
      return;
    }
    const int adj_amount = amount & 31;
    operand = (operand >> adj_amount) | (operand << ((32 - adj_amount) & 31));
    carry = operand >> 31;
  }
}

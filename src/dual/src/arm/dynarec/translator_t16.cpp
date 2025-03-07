
#include <atom/bit.hpp>

#include "translator_t16.hpp"

bool debug_print_ir = false;

namespace bit = atom::bit;

namespace dual::arm::jit {

TranslatorT16::TranslatorT16() {
  BuildLUT();
}

TranslatorT16::Code TranslatorT16::Translate(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  const size_t hash = instruction >> 8;
  return (this->*m_handler_lut[hash])(r15, cpu_mode, instruction, emitter);
}

TranslatorT16::Code TranslatorT16::Translate_ShiftByImm(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  enum class Shift { LSL = 0, LSR = 1, ASR = 2 };

  const ir::GPR reg_dst = bit::get_field<u16, ir::GPR>(instruction, 0u, 3u);
  const ir::GPR reg_src = bit::get_field<u16, ir::GPR>(instruction, 3u, 3u);
  const int imm_shift = bit::get_field<u16, int>(instruction, 6u, 5u);
  const Shift shift_op = bit::get_field<u16, Shift>(instruction, 11u, 2u);

  const ir::U32Value& src_value = emitter.LDGPR(reg_src, cpu_mode);
  const ir::U32Value* result_value = &src_value;

  // TODO(fleroviux): turn this into a reusable function
  const ir::HostFlagsValue* hflag_value;
  switch(shift_op) {
    case Shift::LSL: {
      if(imm_shift != 0) {
        result_value = &emitter.LSL(src_value, emitter.LDCONST((u32)imm_shift), &hflag_value);
        UpdateFlags(emitter, Flags::C, *hflag_value);
      }
      break;
    }
    case Shift::LSR: {
      // LSR #32 is encoded as LSR #0
      const u32 shift_amount = imm_shift == 0 ? 32 : imm_shift;
      result_value = &emitter.LSR(src_value, emitter.LDCONST(shift_amount), &hflag_value);
      UpdateFlags(emitter, Flags::C, *hflag_value);
      break;
    }
    case Shift::ASR: {
      // ASR #32 is encoded as ASR #0
      const u32 shift_amount = imm_shift == 0 ? 32 : imm_shift;
      result_value = &emitter.ASR(src_value, emitter.LDCONST(shift_amount), &hflag_value);
      UpdateFlags(emitter, Flags::C, *hflag_value);
      break;
    }
    default: {
      // shift_op == 0b11 is used to encode ""Add/subtract register or immediate" instructions
      ATOM_UNREACHABLE()
    }
  }

  // Update NZ flags
  UpdateFlags(emitter, Flags::NZ, emitter.TST(*result_value, *result_value));

  // Store the result in the destination register
  emitter.STGPR(reg_dst, cpu_mode, *result_value);

  AdvancePC(emitter, r15);
  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_AddSubtract(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  enum class Opcode { ADD = 0, SUB = 1 };

  const ir::GPR reg_dst = bit::get_field<u16, ir::GPR>(instruction, 0u, 3u);
  const ir::GPR reg_lhs = bit::get_field<u16, ir::GPR>(instruction, 3u, 3u);
  const Opcode opcode = bit::get_bit<u16, Opcode>(instruction, 9u);
  const bool immediate = bit::get_bit(instruction, 10u);

  const ir::HostFlagsValue* hflag_value;
  const ir::U32Value* result_value;

  const ir::U32Value& lhs_value = emitter.LDGPR(reg_lhs, cpu_mode);

  const ir::U32Value* rhs_value;
  if(immediate) {
    rhs_value = &emitter.LDCONST(bit::get_field<u16, u32>(instruction, 6u, 3u));
  } else {
    rhs_value = &emitter.LDGPR(bit::get_field<u16, ir::GPR>(instruction, 6u, 3u), cpu_mode);
  }

  switch(opcode) {
    case Opcode::ADD: result_value = &emitter.ADD(lhs_value, *rhs_value, &hflag_value); break;
    case Opcode::SUB: result_value = &emitter.SUB(lhs_value, *rhs_value, &hflag_value); break;
    default: ATOM_UNREACHABLE();
  }

  UpdateFlags(emitter, Flags::NZCV, *hflag_value);
  emitter.STGPR(reg_dst, cpu_mode, *result_value);

  AdvancePC(emitter, r15);
  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_AddSubCmpMovImm(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  enum class Opcode { MOV = 0, CMP = 1, ADD = 2, SUB = 3 };

  const u32 imm = bit::get_field(instruction, 0u, 8u);
  const ir::GPR reg_dst_lhs = bit::get_field<u16, ir::GPR>(instruction, 8u, 3u);
  const Opcode opcode = bit::get_field<u16, Opcode>(instruction, 11u, 2u);

  const ir::U32Value& lhs_value = emitter.LDGPR(reg_dst_lhs, cpu_mode);
  const ir::U32Value& rhs_value = emitter.LDCONST(imm);

  const ir::HostFlagsValue* hflag_value;

  switch(opcode) {
    case Opcode::MOV: {
      const ir::U32Value& nzcv_value = emitter.LDCONST(imm == 0u ? 0x40000000u : 0u);
      UpdateFlags(emitter, Flags::NZ, nzcv_value);
      emitter.STGPR(reg_dst_lhs, cpu_mode, rhs_value);
      break;
    }
    case Opcode::CMP: {
      UpdateFlags(emitter, Flags::NZCV, emitter.CMP(lhs_value, rhs_value));
      break;
    }
    case Opcode::ADD: {
      const ir::U32Value& result_value = emitter.ADD(lhs_value, rhs_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    case Opcode::SUB: {
      const ir::U32Value& result_value = emitter.SUB(lhs_value, rhs_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    default: ATOM_UNREACHABLE();
  }

  AdvancePC(emitter, r15);
  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_DataProcessingReg(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  enum class Opcode {
    AND = 0,  EOR = 1,  LSL = 2,  LSR = 3,
    ASR = 4,  ADC = 5,  SBC = 6,  ROR = 7,
    TST = 8,  NEG = 9,  CMP = 10, CMN = 11,
    ORR = 12, MUL = 13, BIC = 14, MVN = 15
  };

  const ir::GPR reg_dst_lhs = bit::get_field<u16, ir::GPR>(instruction, 0u, 3u);
  const ir::GPR reg_rhs = bit::get_field<u16, ir::GPR>(instruction, 3u, 3u);
  const Opcode opcode = bit::get_field<u16, Opcode>(instruction, 6u, 4u);

  const ir::HostFlagsValue* hflag_value;

  const ir::U32Value& lhs_value = emitter.LDGPR(reg_dst_lhs, cpu_mode);
  const ir::U32Value& rhs_value = emitter.LDGPR(reg_rhs, cpu_mode);

  switch(opcode) {
    case Opcode::AND: {
      const ir::U32Value& result_value = emitter.AND(lhs_value, rhs_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZ, *hflag_value);
      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    case Opcode::EOR: {
      const ir::U32Value& result_value = emitter.EOR(lhs_value, rhs_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZ, *hflag_value);
      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    case Opcode::LSL: {
      // amount_value = min(33, (rhs_value & 0xff))
      const ir::U32Value* amount_value;
      const ir::U32Value& const_33 = emitter.LDCONST(33);
      amount_value = &emitter.AND(rhs_value, emitter.LDCONST(255u));
      amount_value = &emitter.CSEL(ir::Condition::LO, emitter.CMP(*amount_value, const_33), *amount_value, const_33);

      const ir::HostFlagsValue& old_carry_value = emitter.CVT_NZCV_HFLAG(emitter.LDCPSR());
      const ir::HostFlagsValue* new_carry_value;
      const ir::U32Value& result_value = emitter.LSL(lhs_value, *amount_value, &new_carry_value);

      // Update NZC flags:
      // N = sign(result)
      // Z = result == 0
      // C = amount == 0 ? old_carry : new_carry
      UpdateFlags(emitter, Flags::NZ, emitter.TST(result_value, result_value));
      UpdateFlags(emitter, Flags::C,  emitter.CSEL(ir::Condition::EQ, emitter.TST(*amount_value, *amount_value), old_carry_value, *new_carry_value));

      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    case Opcode::LSR: {
      // amount_value = min(33, (rhs_value & 0xff))
      const ir::U32Value* amount_value;
      const ir::U32Value& const_33 = emitter.LDCONST(33);
      amount_value = &emitter.AND(rhs_value, emitter.LDCONST(255u));
      amount_value = &emitter.CSEL(ir::Condition::LO, emitter.CMP(*amount_value, const_33), *amount_value, const_33);

      const ir::HostFlagsValue& old_carry_value = emitter.CVT_NZCV_HFLAG(emitter.LDCPSR());
      const ir::HostFlagsValue* new_carry_value;
      const ir::U32Value& result_value = emitter.LSR(lhs_value, *amount_value, &new_carry_value);

      // Update NZC flags:
      // N = sign(result)
      // Z = result == 0
      // C = amount == 0 ? old_carry : new_carry
      UpdateFlags(emitter, Flags::NZ, emitter.TST(result_value, result_value));
      UpdateFlags(emitter, Flags::C,  emitter.CSEL(ir::Condition::EQ, emitter.TST(*amount_value, *amount_value), old_carry_value, *new_carry_value));

      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    case Opcode::ASR: {
      // amount_value = min(33, (rhs_value & 0xff))
      const ir::U32Value* amount_value;
      const ir::U32Value& const_33 = emitter.LDCONST(33);
      amount_value = &emitter.AND(rhs_value, emitter.LDCONST(255u));
      amount_value = &emitter.CSEL(ir::Condition::LO, emitter.CMP(*amount_value, const_33), *amount_value, const_33);

      const ir::HostFlagsValue& old_carry_value = emitter.CVT_NZCV_HFLAG(emitter.LDCPSR());
      const ir::HostFlagsValue* new_carry_value;
      const ir::U32Value& result_value = emitter.ASR(lhs_value, *amount_value, &new_carry_value);

      // Update NZC flags:
      // N = sign(result)
      // Z = result == 0
      // C = amount == 0 ? old_carry : new_carry
      UpdateFlags(emitter, Flags::NZ, emitter.TST(result_value, result_value));
      UpdateFlags(emitter, Flags::C,  emitter.CSEL(ir::Condition::EQ, emitter.TST(*amount_value, *amount_value), old_carry_value, *new_carry_value));

      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      debug_print_ir = true;
      break;
    }
    case Opcode::ADC: {
      const ir::HostFlagsValue& carry_in_value = emitter.CVT_NZCV_HFLAG(emitter.LDCPSR());
      const ir::U32Value& result_value = emitter.ADC(lhs_value, rhs_value, carry_in_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    case Opcode::SBC: {
      const ir::HostFlagsValue& carry_in_value = emitter.CVT_NZCV_HFLAG(emitter.LDCPSR());
      const ir::U32Value& result_value = emitter.SBC(lhs_value, rhs_value, carry_in_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    case Opcode::ROR: return Code::Fallback;
    case Opcode::TST: {
      UpdateFlags(emitter, Flags::NZ, emitter.TST(lhs_value, rhs_value));
      break;
    }
    case Opcode::NEG: {
      const ir::U32Value& result_value = emitter.SUB(emitter.LDCONST(0u), rhs_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    case Opcode::CMP: {
      UpdateFlags(emitter, Flags::NZCV, emitter.CMP(lhs_value, rhs_value));
      break;
    }
    case Opcode::CMN: {
      UpdateFlags(emitter, Flags::NZCV, emitter.CMN(lhs_value, rhs_value));
      break;
    }
    case Opcode::ORR: {
      const ir::U32Value& result_value = emitter.ORR(lhs_value, rhs_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZ, *hflag_value);
      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    case Opcode::MUL: {
      const ir::U32Value& result_value = emitter.MUL(lhs_value, rhs_value);
      emitter.AND(result_value, result_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZ, *hflag_value);
      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    case Opcode::BIC: {
      const ir::U32Value& result_value = emitter.BIC(lhs_value, rhs_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZ, *hflag_value);
      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    case Opcode::MVN: {
      const ir::U32Value& result_value = emitter.NOT(rhs_value);
      UpdateFlags(emitter, Flags::NZ, emitter.TST(result_value, result_value));
      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    }
    default: ATOM_UNREACHABLE();
  }

  AdvancePC(emitter, r15);
  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_SpecialDataProcessing(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  enum class Opcode { ADD = 0, CMP = 1, MOV = 2 };

  // TODO(fleroviux): remove the weird PC alignment stuff? Seems potentially unnecessary.
  // Also simplify pipeline flushes?

  int reg_dst_lhs = bit::get_field(instruction, 0u, 3u);
  int reg_rhs = bit::get_field(instruction, 3u, 3u);
  const Opcode opcode = bit::get_field<u16, Opcode>(instruction, 8u, 2u);

  // Instruction may access higher registers r8 - r15 ("Hi register").
  // This is archieved using two extra bits that displace the register number by 8.
  if(bit::get_bit(instruction, 7u)) reg_dst_lhs |= 8;
  if(bit::get_bit(instruction, 6u)) reg_rhs     |= 8;

  const ir::U32Value& lhs_value = emitter.LDGPR((ir::GPR)reg_dst_lhs, cpu_mode);

  const ir::U32Value* rhs_value = &emitter.LDGPR((ir::GPR)reg_rhs, cpu_mode);
  if(reg_rhs == 15) {
    rhs_value = &emitter.BIC(*rhs_value, emitter.LDCONST(1u));
  }

  switch(opcode) {
    case Opcode::ADD: {
      const ir::U32Value* result_value = &emitter.ADD(lhs_value, *rhs_value);

      if(reg_dst_lhs == 15) {
        // Reload pipeline and force-align PC
        result_value = &emitter.ADD(*result_value, emitter.LDCONST(4u));
        result_value = &emitter.BIC(*result_value, emitter.LDCONST(1u));
        emitter.STGPR(ir::GPR::PC, cpu_mode, *result_value);
      } else {
        emitter.STGPR((ir::GPR)reg_dst_lhs, cpu_mode, *result_value);
        AdvancePC(emitter, r15);
      }
      break;
    }
    case Opcode::CMP: {
      if(reg_dst_lhs == 15) {
        ATOM_PANIC("buggy CMP r15 in thumb mode");
      }
      const ir::HostFlagsValue* hflag_value;
      emitter.SUB(lhs_value, *rhs_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      AdvancePC(emitter, r15);
      break;
    }
    case Opcode::MOV: {
      if(reg_dst_lhs == 15) {
        // Reload pipeline and force-align PC
        const ir::U32Value& address_value = emitter.BIC(emitter.ADD(*rhs_value, emitter.LDCONST(4u)), emitter.LDCONST(1u));
        emitter.STGPR(ir::GPR::PC, cpu_mode, address_value);
      } else {
        emitter.STGPR((ir::GPR)reg_dst_lhs, cpu_mode, *rhs_value);
        AdvancePC(emitter, r15);
      }
      break;
    }
    default: ATOM_UNREACHABLE();
  }

  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_Unimplemented(u32, ir::Mode, u16, ir::Emitter&) {
  return Code::Fallback;
}

void TranslatorT16::AdvancePC(ir::Emitter& emitter, u32 current_r15) {
  emitter.STGPR(ir::GPR::PC, ir::Mode::System, emitter.LDCONST(current_r15 + sizeof(u16)));
}

void TranslatorT16::UpdateFlags(ir::Emitter& emitter, u32 flag_set, const ir::HostFlagsValue& hflag_value) {
  const ir::U32Value& nzcv_value = emitter.CVT_HFLAG_NZCV(hflag_value);
  UpdateFlags(emitter, flag_set, nzcv_value);
}

void TranslatorT16::UpdateFlags(ir::Emitter& emitter, u32 flag_set, const ir::U32Value& nzcv_value) {
  const ir::U32Value& cpsr_old = emitter.LDCPSR();
  const ir::U32Value& cpsr_new = emitter.BITCMB(cpsr_old, nzcv_value, flag_set);
  emitter.STCPSR(cpsr_new);
}

void TranslatorT16::BuildLUT() {
  for(u16 hash = 0u; hash < 0x100u; hash++) {
    const u16 instruction = hash << 8;
    m_handler_lut[hash] = GetInstructionHandler(instruction);
  }
}

TranslatorT16::HandlerFn TranslatorT16::GetInstructionHandler(u16 instruction) {
  #define DECODE(pattern, handler) { \
    if(bit::match_pattern<pattern>(instruction)) { \
      return &TranslatorT16::Translate_##handler; \
    } \
  }

  // TODO(fleroviux): remove legend stuff, since we don't use it?

  /**
   * Legend:
   * o = Opcode
   * i = Immediate, Offset, PC-relative offset, SP-relative offset
   * m = Rm
   * n = Rn
   * d = Rd
   * s = Rs
   * c = condition
   * r = register list
   * B = byte-bit
   * L = link-bit, load-bit
   * R = push r14/pop r15
   */
  DECODE("000110ommmnnnddd", AddSubtract) // Add/subtract register
  DECODE("000111oiiinnnddd", AddSubtract) // Add/subtract immediate
  DECODE("000ooiiiiimmmddd", ShiftByImm) // Shift by immediate
  DECODE("001oonnniiiiiiii", AddSubCmpMovImm) // Add/subtract/compare/move immediate, NOTE: nnn is nnn and/or ddd
  DECODE("010000oooosssddd", DataProcessingReg) // Data-processing register, NOTE: sss = Rm/Rs, ddd = Rd/Rn
  DECODE("01000111LYmmmddd", Unimplemented) // Branch/exchange instruction set, Y = H2
  DECODE("010001ooXYmmmddd", SpecialDataProcessing) // Special data processing, X=H1, Y=H2, ddd = Rd/Rn
  DECODE("01001dddiiiiiiii", Unimplemented) // Load from literal pool
  DECODE("0101oo0mmmnnnddd", Unimplemented) // Load/store register offset
  DECODE("0101oo1mmmnnnddd", Unimplemented) // Load/store signed, FIXME: merge this with the format above
  DECODE("011BLiiiiinnnddd", Unimplemented) // Load/store word/byte immediate offset
  DECODE("1000Liiiiinnnddd", Unimplemented) // Load/store halfword immediate offset
  DECODE("1001Ldddiiiiiiii", Unimplemented) // Load/store to/from stack
  DECODE("1010Xdddiiiiiiii", Unimplemented) // Add to SP or PC, X = SP
  DECODE("10110000oiiiiiii", Unimplemented) // Adjust stack pointer
  DECODE("1011L10Rrrrrrrrr", Unimplemented) // Push/pop register list
  DECODE("10111110iiiiiiii", Unimplemented) // Software breakpoint
  DECODE("1100Lnnnrrrrrrrr", Unimplemented) // Load/store multiple
  DECODE("11011110xxxxxxxx", Unimplemented) // Undefined instruction
  DECODE("11011111iiiiiiii", Unimplemented) // Software interrupt
  DECODE("1101cccciiiiiiii", Unimplemented) // Conditional branch
  DECODE("11100iiiiiiiiiii", Unimplemented) // Unconditional branch
  DECODE("11101xxxxxxxxxx1", Unimplemented) // Undefined instruction
  DECODE("11101iiiiiiiiiii", Unimplemented) // BLX suffix
  DECODE("11110iiiiiiiiiii", Unimplemented) // BL/BLX prefix
  DECODE("11111iiiiiiiiiii", Unimplemented) // BL suffix

  #undef DECODE

  return &TranslatorT16::Translate_Unimplemented;
}

} // namespace dual::arm::jit

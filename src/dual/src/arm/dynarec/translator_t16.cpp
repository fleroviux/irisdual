
#include <atom/bit.hpp>
#include <bit>

#include "translator_t16.hpp"

bool debug_print_ir = false;

namespace bit = atom::bit;

namespace dual::arm::jit {

TranslatorT16::TranslatorT16(CPU::Model cpu_model) : m_cpu_model{cpu_model} {
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
    case Opcode::ROR: {
      const ir::U32Value& amount_value = emitter.AND(rhs_value, emitter.LDCONST(31u));

      const ir::HostFlagsValue& old_carry_value = emitter.CVT_NZCV_HFLAG(emitter.LDCPSR());
      const ir::HostFlagsValue* new_carry_value;
      const ir::U32Value& result_value = emitter.ROR(lhs_value, amount_value, &new_carry_value);

      // Update NZC flags:
      // N = sign(result)
      // Z = result == 0
      // C = amount == 0 ? old_carry : new_carry
      UpdateFlags(emitter, Flags::NZ, emitter.TST(result_value, result_value));
      UpdateFlags(emitter, Flags::C,  emitter.CSEL(ir::Condition::EQ, emitter.TST(amount_value, amount_value), old_carry_value, *new_carry_value));

      emitter.STGPR(reg_dst_lhs, cpu_mode, result_value);
      break;
    };
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
      const ir::U32Value& result_value = emitter.ADD(lhs_value, *rhs_value);

      if(reg_dst_lhs == 15) {
        // Reload pipeline and force-align PC
        Flush(emitter, emitter.BIC(result_value, emitter.LDCONST(1u)));
      } else {
        emitter.STGPR((ir::GPR)reg_dst_lhs, cpu_mode, result_value);
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
        Flush(emitter, emitter.BIC(*rhs_value, emitter.LDCONST(1u)));
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

TranslatorT16::Code TranslatorT16::Translate_LoadFromLiteralPool(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  // TODO(fleroviux): test if the address always is force-aligned (at least on ARM7?) or if we should actually perform a rotated word read.
  const u32 address = (r15 & ~2) + bit::get_field(instruction, 0u, 8u) * sizeof(u32);
  const ir::GPR reg_dst = bit::get_field<u16, ir::GPR>(instruction, 8u, 3u);
  emitter.STGPR(reg_dst, cpu_mode, emitter.LDR(emitter.LDCONST(address)));
  AdvancePC(emitter, r15);
  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_LoadStoreRegOffset(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  enum class Opcode {
    STR = 0, STRH = 1, STRB = 2, LDRSB = 3,
    LDR = 4, LDRH = 5, LDRB = 6, LDRSH = 7
  };

  const ir::GPR reg_src_dst = bit::get_field<u16, ir::GPR>(instruction, 0u, 3u);
  const ir::GPR reg_base = bit::get_field<u16, ir::GPR>(instruction, 3u, 3u);
  const ir::GPR reg_offset = bit::get_field<u16, ir::GPR>(instruction, 6u, 3u);
  const Opcode opcode = bit::get_field<u16, Opcode>(instruction, 9u, 3u);

  const ir::U32Value& address_value = emitter.ADD(emitter.LDGPR(reg_base, cpu_mode), emitter.LDGPR(reg_offset, cpu_mode));

  switch(opcode) {
    case Opcode::STR: {
      emitter.STR(address_value, emitter.LDGPR(reg_src_dst, cpu_mode));
      break;
    }
    case Opcode::STRH: {
      emitter.STRH(address_value, emitter.LDGPR(reg_src_dst, cpu_mode));
      break;
    }
    case Opcode::STRB: {
      emitter.STRB(address_value, emitter.LDGPR(reg_src_dst, cpu_mode));
      break;
    }
    case Opcode::LDRSB: {
      emitter.STGPR(reg_src_dst, cpu_mode, emitter.SXTB(emitter.LDRB(address_value)));
      break;
    }
    case Opcode::LDR: {
      emitter.STGPR(reg_src_dst, cpu_mode, ReadWordRotate(emitter, address_value));
      break;
    }
    case Opcode::LDRH: {
      emitter.STGPR(reg_src_dst, cpu_mode, ReadHalfMaybeRotate(emitter, address_value));
      break;
    }
    case Opcode::LDRB: {
      emitter.STGPR(reg_src_dst, cpu_mode, emitter.LDRB(address_value));
      break;
    }
    case Opcode::LDRSH: {
      emitter.STGPR(reg_src_dst, cpu_mode, emitter.SXTH(ReadHalfMaybeRotate(emitter, address_value)));
      break;
    }
    default: ATOM_UNREACHABLE();
  }

  AdvancePC(emitter, r15);
  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_LoadStoreWordByteImmOffset(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  enum class Opcode { STR = 0, LDR = 1, STRB = 2, LDRB = 3 };

  const ir::GPR reg_src_dst = bit::get_field<u16, ir::GPR>(instruction, 0u, 3u);
  const ir::GPR reg_base = bit::get_field<u16, ir::GPR>(instruction, 3u, 3u);
  const u32 imm_offset = bit::get_field(instruction, 6u, 5u);
  const Opcode opcode = bit::get_field<u16, Opcode>(instruction, 11u, 2u);

  const ir::U32Value& base_address_value = emitter.LDGPR(reg_base, cpu_mode);

  switch(opcode) {
    case Opcode::STR: {
      const ir::U32Value& address_value = emitter.ADD(base_address_value, emitter.LDCONST(imm_offset * sizeof(u32)));
      emitter.STR(address_value, emitter.LDGPR(reg_src_dst, cpu_mode));
      break;
    }
    case Opcode::LDR: {
      const ir::U32Value& address_value = emitter.ADD(base_address_value, emitter.LDCONST(imm_offset * sizeof(u32)));
      emitter.STGPR(reg_src_dst, cpu_mode, ReadWordRotate(emitter, address_value));
      break;
    }
    case Opcode::STRB: {
      const ir::U32Value& address_value = emitter.ADD(base_address_value, emitter.LDCONST(imm_offset));
      emitter.STRB(address_value, emitter.LDGPR(reg_src_dst, cpu_mode));
      break;
    }
    case Opcode::LDRB: {
      const ir::U32Value& address_value = emitter.ADD(base_address_value, emitter.LDCONST(imm_offset));
      emitter.STGPR(reg_src_dst, cpu_mode, emitter.LDRB(address_value));
      break;
    }
    default: ATOM_UNREACHABLE();
  }

  AdvancePC(emitter, r15);
  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_LoadStoreHalfImmOffset(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  const ir::GPR reg_src_dst = bit::get_field<u16, ir::GPR>(instruction, 0u, 3u);
  const ir::GPR reg_base = bit::get_field<u16, ir::GPR>(instruction, 3u, 3u);
  const u32 imm_offset = bit::get_field(instruction, 6u, 5u);

  const ir::U32Value& address_value = emitter.ADD(emitter.LDGPR(reg_base, cpu_mode), emitter.LDCONST(imm_offset * sizeof(u16)));

  if(bit::get_bit(instruction, 11u)) {
    emitter.STGPR(reg_src_dst, cpu_mode, ReadHalfMaybeRotate(emitter, address_value));
  } else {
    emitter.STRH(address_value, emitter.LDGPR(reg_src_dst, cpu_mode));
  }

  AdvancePC(emitter, r15);
  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_LoadStoreToFromStack(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  const u32 imm_offset = bit::get_field(instruction, 0u, 8u);
  const ir::GPR reg_src_dst = bit::get_field<u16, ir::GPR>(instruction, 8u, 3u);

  const ir::U32Value& address_value = emitter.ADD(emitter.LDGPR(ir::GPR::SP, cpu_mode), emitter.LDCONST(imm_offset * sizeof(u32)));

  if(bit::get_bit(instruction, 11u)) {
    emitter.STGPR(reg_src_dst, cpu_mode, ReadWordRotate(emitter, address_value));
  } else {
    emitter.STR(address_value, emitter.LDGPR(reg_src_dst, cpu_mode));
  }

  AdvancePC(emitter, r15);
  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_AddToSPOrPC(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  const u32 imm_rhs = bit::get_field(instruction, 0u, 8u);
  const ir::GPR reg_dst = bit::get_field<u16, ir::GPR>(instruction, 8u, 3u);

  // TODO(fleroviux): test if bit 1 of R15 really is forced to zero and on what processor models?
  const ir::U32Value& lhs_value = bit::get_bit(instruction, 11u) ?  emitter.LDGPR(ir::GPR::SP, cpu_mode) : emitter.LDCONST(r15 & ~2);
  const ir::U32Value& rhs_value = emitter.LDCONST(imm_rhs * sizeof(u32));
  emitter.STGPR(reg_dst, cpu_mode, emitter.ADD(lhs_value, rhs_value));

  AdvancePC(emitter, r15);
  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_AdjustStackPointer(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  enum class Opcode { ADD = 0, SUB = 1 };

  const u32 imm_rhs = bit::get_field(instruction, 0u, 7u);
  const Opcode opcode = bit::get_bit<u16, Opcode>(instruction, 7u);

  const ir::U32Value& lhs_value = emitter.LDGPR(ir::GPR::SP, cpu_mode);
  const ir::U32Value& rhs_value = emitter.LDCONST(imm_rhs * sizeof(u32));

  switch(opcode) {
    case Opcode::ADD: emitter.STGPR(ir::GPR::SP, cpu_mode, emitter.ADD(lhs_value, rhs_value)); break;
    case Opcode::SUB: emitter.STGPR(ir::GPR::SP, cpu_mode, emitter.SUB(lhs_value, rhs_value)); break;
    default: ATOM_UNREACHABLE();
  }

  AdvancePC(emitter, r15);
  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_PushPopRegList(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  const bool load = bit::get_bit(instruction, 11u);

  // TODO(fleroviux): handle empty register list edge cases (either throw error or emulate them)

  if(load) {
    const ir::U32Value* address_value = &emitter.LDGPR(ir::GPR::SP, cpu_mode);

    for(int reg = 0; reg <= 7; reg++) {
      if(bit::get_bit(instruction, reg)) {
        emitter.STGPR((ir::GPR)reg, cpu_mode, emitter.LDR(*address_value));
        address_value = &emitter.ADD(*address_value, emitter.LDCONST(4u));
      }
    }

    if(bit::get_bit(instruction, 8u)) {
      const ir::U32Value& new_pc_value = emitter.LDR(*address_value);
      if(m_cpu_model != CPU::Model::ARM7) {
        FlushExchange(emitter, new_pc_value);
      } else {
        Flush(emitter, new_pc_value);
      }
      address_value = &emitter.ADD(*address_value, emitter.LDCONST(4u));
    } else {
      AdvancePC(emitter, r15);
    }

    emitter.STGPR(ir::GPR::SP, cpu_mode, *address_value);
  } else {
    const u32 sp_offset = std::popcount(bit::get_field(instruction, 0u, 9u)) * sizeof(u32);
    const ir::U32Value* address_value = &emitter.SUB(emitter.LDGPR(ir::GPR::SP, cpu_mode), emitter.LDCONST(sp_offset));

    emitter.STGPR(ir::GPR::SP, cpu_mode, *address_value);

    for(int reg = 0; reg <= 7; reg++) {
      if(bit::get_bit(instruction, reg)) {
        emitter.STR(*address_value, emitter.LDGPR((ir::GPR)reg, cpu_mode));
        address_value = &emitter.ADD(*address_value, emitter.LDCONST(4u));
      }
    }

    if(bit::get_bit(instruction, 8u)) {
      emitter.STR(*address_value, emitter.LDGPR(ir::GPR::LR, cpu_mode));
    }

    AdvancePC(emitter, r15);
  }

  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_Unimplemented(u32, ir::Mode, u16, ir::Emitter&) {
  return Code::Fallback;
}

void TranslatorT16::AdvancePC(ir::Emitter& emitter, u32 current_r15) {
  emitter.STGPR(ir::GPR::PC, ir::Mode::System, emitter.LDCONST(current_r15 + sizeof(u16)));
}

void TranslatorT16::FlushExchange(ir::Emitter& emitter, const ir::U32Value& new_pc_value) {
  const ir::U32Value& thumb_bit_value = emitter.AND(new_pc_value, emitter.LDCONST(1u));

  // Update R15
  // R15 = (new_pc_value & ~1) + 8 - thumb_bit * 4
  const ir::U32Value& pc_offset_value = emitter.SUB(emitter.LDCONST(8u), emitter.LSL(thumb_bit_value, emitter.LDCONST(2u)));
  emitter.STGPR(ir::GPR::PC, ir::Mode::System, emitter.ADD(emitter.BIC(new_pc_value, emitter.LDCONST(1u)), pc_offset_value));

  // Update CPSR
  // CPSR = CPSR & ~(1 << 5) | thumb_bit << 5
  const ir::U32Value& cpsr_bic_thumb_bit = emitter.BIC(emitter.LDCPSR(), emitter.LDCONST(0x20u));
  const ir::U32Value& cpsr_orr_thumb_bit = emitter.ORR(cpsr_bic_thumb_bit, emitter.LSL(thumb_bit_value, emitter.LDCONST(5u)));
  emitter.STCPSR(cpsr_orr_thumb_bit);
}

void TranslatorT16::Flush(ir::Emitter& emitter, const ir::U32Value& new_pc_value) {
  emitter.STGPR(ir::GPR::PC, ir::Mode::System, emitter.ADD(new_pc_value, emitter.LDCONST(4u)));
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

const ir::U32Value& TranslatorT16::ReadHalfMaybeRotate(ir::Emitter& emitter, const ir::U32Value& address_value) {
  if(m_cpu_model == CPU::Model::ARM7) {
    const ir::U32Value& rotate_amount = emitter.LSL(emitter.AND(address_value, emitter.LDCONST(1u)), emitter.LDCONST(3u));
    return emitter.ROR(emitter.LDRH(address_value), rotate_amount);
  }
  return emitter.LDRH(address_value);
}

const ir::U32Value& TranslatorT16::ReadWordRotate(ir::Emitter& emitter, const ir::U32Value& address_value) {
  // TODO(fleroviux): on ARM11: disable rotate behavior when unaligned data access is enabled.
  const ir::U32Value& rotate_amount = emitter.LSL(emitter.AND(address_value, emitter.LDCONST(3u)), emitter.LDCONST(3u));
  return emitter.ROR(emitter.LDR(address_value), rotate_amount);
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
  DECODE("01001dddiiiiiiii", LoadFromLiteralPool) // Load from literal pool
  DECODE("0101ooommmnnnddd", LoadStoreRegOffset) // Load/store register offset
  DECODE("011BLiiiiinnnddd", LoadStoreWordByteImmOffset) // Load/store word/byte immediate offset
  DECODE("1000Liiiiinnnddd", LoadStoreHalfImmOffset) // Load/store halfword immediate offset
  DECODE("1001Ldddiiiiiiii", LoadStoreToFromStack) // Load/store to/from stack
  DECODE("1010Xdddiiiiiiii", AddToSPOrPC) // Add to SP or PC, X = SP
  DECODE("10110000oiiiiiii", AdjustStackPointer) // Adjust stack pointer
  DECODE("1011L10Rrrrrrrrr", PushPopRegList) // Push/pop register list
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


#include <atom/bit.hpp>
#include <atom/panic.hpp>

#include "translator_a32.hpp"

namespace bit = atom::bit;

namespace dual::arm::jit {

TranslatorA32::TranslatorA32() {
  BuildLUT();
}

TranslatorA32::Code TranslatorA32::Translate(u32 r15, CPU::Mode cpu_mode, u32 instruction, ir::Emitter& emitter) {
  // Bail out on unconditional instructions for now, those need to be handled with special care
  if((instruction >> 28) == 15) {
    return Code::Fallback;
  }

  // Also bail out on conditionally executed instructions
  if((instruction >> 28) != 14) {
    return Code::Fallback;
  }

  const size_t hash = (instruction >> 16) & 0xFF0u | (instruction >> 4) & 0xFu;

  return (this->*m_handler_lut[hash])(r15, cpu_mode, instruction, emitter);
}

TranslatorA32::Code TranslatorA32::Translate_DataProcessing(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter) {
  const ir::GPR reg_dst = bit::get_field<u32, ir::GPR>(instruction, 12u, 4u);
  const ir::GPR reg_lhs = bit::get_field<u32, ir::GPR>(instruction, 16u, 4u);
  const bool set_flags = bit::get_bit(instruction, 20);
  const DataOp opcode = bit::get_field<u32, DataOp>(instruction, 21u, 4u);
  const bool immediate = bit::get_bit(instruction, 25);

  const ir::U32Value& lhs_value = emitter.LDGPR((ir::GPR)reg_lhs, cpu_mode);
  const ir::U32Value* rhs_value;

  // TODO(fleroviux): shifter implementation notes:
  // - Shift operation does not always update carry flag! I.e. LSL #0 (even with register specified shift) does not!
  // - ADC and friends always use the *original* carry flag
  // - Shifter carry is only output if the ALU op itself does not output a carry!

  if(immediate) {
    const int imm_shift = bit::get_field<u32, int>(instruction, 8u, 4u) * 2;
    u32 imm_rhs = bit::get_field(instruction, 0u, 8u);

    if(imm_shift != 0) {
      if(set_flags) {
        // Update carry flag
        // TODO(fleroviux): try to merge this with the flag update later.
        const ir::U32Value &nzcv_value = emitter.LDCONST(imm_rhs >> (imm_shift - 1) << 29);
        UpdateFlags(emitter, Flags::C, nzcv_value);
      }

      imm_rhs = bit::rotate_right(imm_rhs, imm_shift);
    }

    rhs_value = &emitter.LDCONST(imm_rhs);
  } else {
    const bool use_reg_shift = bit::get_bit(instruction, 4);
    const Shift shift_op = bit::get_field<u32, Shift>(instruction, 5u, 2u);

    // TODO(fleroviux): implement the barrel shifter
    if(use_reg_shift) {
      return Code::Fallback;
    } else {
      const int imm_shift = bit::get_field<u32, int>(instruction, 7u, 5u);
      if(shift_op != Shift::LSL || imm_shift != 0) {
        return Code::Fallback;
      }
    }

    const ir::GPR reg_rhs = bit::get_field<u32, ir::GPR>(instruction, 0u, 4u);
    rhs_value = &emitter.LDGPR(reg_rhs, cpu_mode);
  }

  const ir::HostFlagsValue* hflag_value;

  switch(opcode) {
    case DataOp::AND: {
      if(set_flags) {
        emitter.STGPR(reg_dst, cpu_mode, emitter.AND(lhs_value, *rhs_value, &hflag_value));
        UpdateFlags(emitter, Flags::NZ, *hflag_value);
      } else {
        emitter.STGPR(reg_dst, cpu_mode, emitter.AND(lhs_value, *rhs_value));
      }
      break;
    }
    case DataOp::EOR: {
      if(set_flags) {
        emitter.STGPR(reg_dst, cpu_mode, emitter.EOR(lhs_value, *rhs_value, &hflag_value));
        UpdateFlags(emitter, Flags::NZ, *hflag_value);
      } else {
        emitter.STGPR(reg_dst, cpu_mode, emitter.EOR(lhs_value, *rhs_value));
      }
      break;
    }
    case DataOp::SUB: {
      if(set_flags) {
        emitter.STGPR(reg_dst, cpu_mode, emitter.SUB(lhs_value, *rhs_value, &hflag_value));
        UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      } else {
        emitter.STGPR(reg_dst, cpu_mode, emitter.SUB(lhs_value, *rhs_value));
      }
      break;
    }
    case DataOp::RSB: {
      if(set_flags) {
        emitter.STGPR(reg_dst, cpu_mode, emitter.SUB(*rhs_value, lhs_value, &hflag_value));
        UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      } else {
        emitter.STGPR(reg_dst, cpu_mode, emitter.SUB(*rhs_value, lhs_value));
      }
      break;
    }
    case DataOp::ADD: {
      if(set_flags) {
        emitter.STGPR(reg_dst, cpu_mode, emitter.ADD(lhs_value, *rhs_value, &hflag_value));
        UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      } else {
        emitter.STGPR(reg_dst, cpu_mode, emitter.ADD(lhs_value, *rhs_value));
      }
      break;
    }
    case DataOp::ADC: {
      const ir::HostFlagsValue& carry_in = emitter.CVT_NZCV_HFLAG(emitter.LDCPSR());
      if(set_flags) {
        emitter.STGPR(reg_dst, cpu_mode, emitter.ADC(lhs_value, *rhs_value, carry_in, &hflag_value));
        UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      } else {
        emitter.STGPR(reg_dst, cpu_mode, emitter.ADC(lhs_value, *rhs_value, carry_in));
      }
      break;
    }
    case DataOp::SBC: {
      const ir::HostFlagsValue& carry_in = emitter.CVT_NZCV_HFLAG(emitter.LDCPSR());
      if(set_flags) {
        emitter.STGPR(reg_dst, cpu_mode, emitter.SBC(lhs_value, *rhs_value, carry_in, &hflag_value));
        UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      } else {
        emitter.STGPR(reg_dst, cpu_mode, emitter.SBC(lhs_value, *rhs_value, carry_in));
      }
      break;
    }
    case DataOp::RSC: {
      const ir::HostFlagsValue& carry_in = emitter.CVT_NZCV_HFLAG(emitter.LDCPSR());
      if(set_flags) {
        emitter.STGPR(reg_dst, cpu_mode, emitter.SBC(*rhs_value, lhs_value, carry_in, &hflag_value));
        UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      } else {
        emitter.STGPR(reg_dst, cpu_mode, emitter.SBC(*rhs_value, lhs_value, carry_in));
      }
      break;
    }
    case DataOp::TST: {
      emitter.AND(lhs_value, *rhs_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZ, *hflag_value);
      break;
    }
    case DataOp::TEQ: {
      emitter.EOR(lhs_value, *rhs_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZ, *hflag_value);
      break;
    }
    case DataOp::CMP: {
      const ir::HostFlagsValue* hflag_value;
      emitter.SUB(lhs_value, *rhs_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      break;
    }
    case DataOp::CMN: {
      const ir::HostFlagsValue* hflag_value;
      emitter.ADD(lhs_value, *rhs_value, &hflag_value);
      UpdateFlags(emitter, Flags::NZCV, *hflag_value);
      break;
    }
    case DataOp::ORR: {
      if(set_flags) {
        emitter.STGPR(reg_dst, cpu_mode, emitter.ORR(lhs_value, *rhs_value, &hflag_value));
        UpdateFlags(emitter, Flags::NZ, *hflag_value);
      } else {
        emitter.STGPR(reg_dst, cpu_mode, emitter.ORR(lhs_value, *rhs_value));
      }
      break;
    }
    case DataOp::MOV: {
      if(set_flags) {
        emitter.AND(*rhs_value, *rhs_value, &hflag_value);
        UpdateFlags(emitter, Flags::NZ, *hflag_value);
      }
      emitter.STGPR(reg_dst, cpu_mode, *rhs_value);
      break;
    }
    case DataOp::BIC: {
      if(set_flags) {
        emitter.STGPR(reg_dst, cpu_mode, emitter.BIC(lhs_value, *rhs_value, &hflag_value));
        UpdateFlags(emitter, Flags::NZ, *hflag_value);
      } else {
        emitter.STGPR(reg_dst, cpu_mode, emitter.BIC(lhs_value, *rhs_value));
      }
      break;
    }
    case DataOp::MVN: {
      const ir::U32Value& neg_rhs_value = emitter.NOT(*rhs_value);
      if(set_flags) {
        emitter.AND(neg_rhs_value, neg_rhs_value, &hflag_value);
        UpdateFlags(emitter, Flags::NZ, *hflag_value);
      }
      emitter.STGPR(reg_dst, cpu_mode, neg_rhs_value);
      break;
    }
    default: {
      ATOM_PANIC("unknown data processing opcode: {}", (int)opcode);
    }
  }

  if(reg_dst == ir::GPR::PC) {
    return Code::Fallback;
  }

  emitter.STGPR(ir::GPR::PC, cpu_mode, emitter.LDCONST(r15 + 4u));
  return Code::Success;
}

TranslatorA32::Code TranslatorA32::Translate_MRS(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter) {
  const bool use_spsr = bit::get_bit(instruction, 22);
  const ir::GPR reg_dst = bit::get_field<u32, ir::GPR>(instruction, 12u, 4u);

  if(reg_dst == ir::GPR::PC) {
    ATOM_PANIC("illegal write to R15 in A32 MRS");
  }

  if(use_spsr) {
    emitter.STGPR(reg_dst, cpu_mode, emitter.LDSPSR(cpu_mode));
  } else {
    emitter.STGPR(reg_dst, cpu_mode, emitter.LDCPSR());
  }

  emitter.STGPR(ir::GPR::PC, cpu_mode, emitter.LDCONST(r15 + 4u));
  return Code::Success;
}

TranslatorA32::Code TranslatorA32::Translate_MSR_reg(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter) {
  const ir::GPR reg_src = bit::get_field<u32, ir::GPR>(instruction, 0u, 4u);
  const ir::U32Value& reg_value = emitter.LDGPR(reg_src, cpu_mode);

  // TODO(fleroviux): mask out any bits which aren't writable (e.g. on ARM7TDMI)
  u32 fsxc_mask = 0u;
  if(bit::get_bit(instruction, 16)) fsxc_mask |= 0x000000FFu;
  if(bit::get_bit(instruction, 17)) fsxc_mask |= 0x0000FF00u;
  if(bit::get_bit(instruction, 18)) fsxc_mask |= 0x00FF0000u;
  if(bit::get_bit(instruction, 19)) fsxc_mask |= 0xFF000000u;

  const bool use_spsr = bit::get_bit(instruction, 22);

  if(use_spsr) {
    // TODO(fleroviux): Bit 4 of PSR registers is forced to one on ARM7TDMI (what about ARM9 and ARM11?)
    const ir::U32Value& psr_old = emitter.LDSPSR(cpu_mode);
    const ir::U32Value& psr_new = emitter.BITCMB(psr_old, reg_value, fsxc_mask);
    emitter.STSPSR(cpu_mode, psr_new);
  } else {
    // In non-privileged mode (user mode): only condition code bits of CPSR can be changed, control bits can't.
    if(cpu_mode == ir::Mode::User) {
      fsxc_mask &= 0xFF000000u;
    }

    // TODO(fleroviux): Bit 4 of PSR registers is forced to one on ARM7TDMI (what about ARM9 and ARM11?)
    const ir::U32Value& psr_old = emitter.LDCPSR();
    const ir::U32Value& psr_new = emitter.BITCMB(psr_old, reg_value, fsxc_mask);
    emitter.STCPSR(psr_new);
  }

  emitter.STGPR(ir::GPR::PC, cpu_mode, emitter.LDCONST(r15 + 4u));
  return Code::Success;
}

TranslatorA32::Code TranslatorA32::Translate_MSR_imm(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter) {
  const int imm_shift = bit::get_field<u32, int>(instruction, 8u, 4u) * 2;
  const u32 imm = bit::rotate_right(bit::get_field(instruction, 0u, 8u), imm_shift);
  const ir::U32Value& imm_value = emitter.LDCONST(imm);

  // TODO(fleroviux): handle duplicate logic between MSR_reg and MSR_imm

  // TODO(fleroviux): mask out any bits which aren't writable (e.g. on ARM7TDMI)
  u32 fsxc_mask = 0u;
  if(bit::get_bit(instruction, 16)) fsxc_mask |= 0x000000FFu;
  if(bit::get_bit(instruction, 17)) fsxc_mask |= 0x0000FF00u;
  if(bit::get_bit(instruction, 18)) fsxc_mask |= 0x00FF0000u;
  if(bit::get_bit(instruction, 19)) fsxc_mask |= 0xFF000000u;

  const bool use_spsr = bit::get_bit(instruction, 22);

  if(use_spsr) {
    // TODO(fleroviux): Bit 4 of PSR registers is forced to one on ARM7TDMI (what about ARM9 and ARM11?)
    const ir::U32Value& psr_old = emitter.LDSPSR(cpu_mode);
    const ir::U32Value& psr_new = emitter.BITCMB(psr_old, imm_value, fsxc_mask);
    emitter.STSPSR(cpu_mode, psr_new);
  } else {
    // In non-privileged mode (user mode): only condition code bits of CPSR can be changed, control bits can't.
    if(cpu_mode == ir::Mode::User) {
      fsxc_mask &= 0xFF000000u;
    }

    // TODO(fleroviux): Bit 4 of PSR registers is forced to one on ARM7TDMI (what about ARM9 and ARM11?)
    const ir::U32Value& psr_old = emitter.LDCPSR();
    const ir::U32Value& psr_new = emitter.BITCMB(psr_old, imm_value, fsxc_mask);
    emitter.STCPSR(psr_new);
  }

  emitter.STGPR(ir::GPR::PC, cpu_mode, emitter.LDCONST(r15 + 4u));
  return Code::Success;
}

TranslatorA32::Code TranslatorA32::Translate_Unimplemented(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter) {
  return Code::Fallback;
}

void TranslatorA32::UpdateFlags(ir::Emitter& emitter, u32 flag_set, const ir::HostFlagsValue& hflag_value) {
  const ir::U32Value& nzcv_value = emitter.CVT_HFLAG_NZCV(hflag_value);
  UpdateFlags(emitter, flag_set, nzcv_value);
}

void TranslatorA32::UpdateFlags(ir::Emitter& emitter, u32 flag_set, const ir::U32Value& nzcv_value) {
  const ir::U32Value& cpsr_old = emitter.LDCPSR();
  const ir::U32Value& cpsr_new = emitter.BITCMB(cpsr_old, nzcv_value, flag_set);
  emitter.STCPSR(cpsr_new);
}

void TranslatorA32::BuildLUT() {
  for(u32 hash = 0u; hash < 0x1000u; hash++) {
    const u32 instruction = (hash & 0xFF0u) << 16 | (hash & 0xFu) << 4;
    m_handler_lut[hash] = GetInstructionHandler(instruction);
  }
}

TranslatorA32::HandlerFn TranslatorA32::GetInstructionHandler(u32 instruction) {
  #define DECODE(pattern, handler) { \
    if(bit::match_pattern<pattern>(instruction)) { \
      return &TranslatorA32::Translate_##handler;  \
    } \
  }

  DECODE("cccc00010x00xxxxxxxxxxxx0000xxxx", MRS)            // Move status register to register
  DECODE("cccc00010x10xxxxxxxxxxxx0000xxxx", MSR_reg)        // Move register to status register
  DECODE("cccc00010xx0xxxxxxxxxxxx1xx0xxxx", Unimplemented)  // Enhanced DSP multiplies
  DECODE("cccc000xxxxxxxxxxxxxxxxxxxx0xxxx", DataProcessing) // Data Processing (Shift-by-Immediate)
  DECODE("cccc00010010xxxxxxxxxxxx0001xxxx", Unimplemented)  // Branch/exchange instruction set
  DECODE("cccc00010110xxxxxxxxxxxx0001xxxx", Unimplemented)  // Count leading zeros
  DECODE("cccc00010010xxxxxxxxxxxx0011xxxx", Unimplemented)  // Branch and link/exchange instruction set
  DECODE("cccc00010xx0xxxxxxxxxxxx0101xxxx", Unimplemented)  // Enhanced DSP add/subtracts
  DECODE("cccc00010010xxxxxxxxxxxx0111xxxx", Unimplemented)  // Software breakpoint
  DECODE("cccc000xxxxxxxxxxxxxxxxx0xx1xxxx", DataProcessing) // Data Processing (Shift-by-Register)
  DECODE("cccc000000xxxxxxxxxxxxxx1001xxxx", Unimplemented)  // Multiply (accumulate)
  DECODE("cccc00001xxxxxxxxxxxxxxx1001xxxx", Unimplemented)  // Multiply (accumulate) long
  DECODE("cccc00010x00xxxxxxxxxxxx1001xxxx", Unimplemented)  // Swap/swap byte
  DECODE("cccc000xx0xxxxxxxxxxxxxx1011xxxx", Unimplemented)  // Load/store halfword register offset
  DECODE("cccc000xx1xxxxxxxxxxxxxx1011xxxx", Unimplemented)  // Load/store halfword immediate offset
  DECODE("cccc000xx0x0xxxxxxxxxxxx11x1xxxx", Unimplemented)  // Load/store two words register offset
  DECODE("cccc000xx0x1xxxxxxxxxxxx11x1xxxx", Unimplemented)  // Load signed halfword/byte register offset
  DECODE("cccc000xx1x0xxxxxxxxxxxx11x1xxxx", Unimplemented)  // Load/store two words immediate offset
  DECODE("cccc000xx1x1xxxxxxxxxxxx11x1xxxx", Unimplemented)  // Load signed halfword/byte register offset
  DECODE("cccc00110x00xxxxxxxxxxxxxxxxxxxx", Unimplemented)  // Undefined instruction (UNPREDICTABLE prior to ARM architecture version 4.)
  DECODE("cccc00110x10xxxxxxxxxxxxxxxxxxxx", MSR_imm)        // Move immediate to status register
  DECODE("cccc001xxxxxxxxxxxxxxxxxxxxxxxxx", DataProcessing) // Data Processing (Immediate)
  DECODE("cccc010xxxxxxxxxxxxxxxxxxxxxxxxx", Unimplemented)  // Load/Store (Immediate Offset)
  DECODE("cccc011xxxxxxxxxxxxxxxxxxxx0xxxx", Unimplemented)  // Load/Store (Register Offset)
  DECODE("cccc011xxxxxxxxxxxxxxxxxxxx1xxxx", Unimplemented)  // Undefined instruction
  DECODE("cccc100xxxxxxxxxxxxxxxxxxxxxxxxx", Unimplemented)  // Load/store multiple
  DECODE("cccc101xxxxxxxxxxxxxxxxxxxxxxxxx", Unimplemented)  // Branch and branch with link
  DECODE("cccc110xxxxxxxxxxxxxxxxxxxxxxxxx", Unimplemented)  // Coprocessor load/store and double register transfers
  DECODE("cccc1110xxxxxxxxxxxxxxxxxxx0xxxx", Unimplemented)  // Coprocessor data processing
  DECODE("cccc1110xxxxxxxxxxxxxxxxxxx1xxxx", Unimplemented)  // Coprocessor register transfers
  DECODE("cccc1111xxxxxxxxxxxxxxxxxxxxxxxx", Unimplemented)  // Software interrupt

  #undef DECODE

  return &TranslatorA32::Translate_Unimplemented;
}

} // namespace dual::arm::jit

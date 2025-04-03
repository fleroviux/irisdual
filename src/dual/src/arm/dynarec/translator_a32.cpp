
#include <atom/bit.hpp>
#include <atom/panic.hpp>
#include <bit>

#include "translator_a32.hpp"

namespace bit = atom::bit;

namespace dual::arm::jit {

TranslatorA32::TranslatorA32(CPU::Model cpu_model) : m_cpu_model{cpu_model} {
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
  // TODO(fleroviux): redo and finish this

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

TranslatorA32::Code TranslatorA32::Translate_MoveStatusRegToReg(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter) {
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

TranslatorA32::Code TranslatorA32::Translate_MoveRegOrImmToStatusReg(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter) {
  const bool immediate = bit::get_bit(instruction, 25u);

  const ir::U32Value* src_value;

  if(immediate) {
    const int imm_shift = bit::get_field<u32, int>(instruction, 8u, 4u) * 2;
    const u32 imm = bit::rotate_right(bit::get_field(instruction, 0u, 8u), imm_shift);
    src_value = &emitter.LDCONST(imm);
  } else {
    const ir::GPR reg_src = bit::get_field<u32, ir::GPR>(instruction, 0u, 4u);
    src_value = &emitter.LDGPR(reg_src, cpu_mode);
  }

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
    const ir::U32Value& psr_new = emitter.BITCMB(psr_old, *src_value, fsxc_mask);
    emitter.STSPSR(cpu_mode, psr_new);
  } else {
    // In non-privileged mode (user mode): only condition code bits of CPSR can be changed, control bits can't.
    if(cpu_mode == ir::Mode::User) {
      fsxc_mask &= 0xFF000000u;
    }

    // TODO(fleroviux): Bit 4 of PSR registers is forced to one on ARM7TDMI (what about ARM9 and ARM11?)
    const ir::U32Value& psr_old = emitter.LDCPSR();
    const ir::U32Value& psr_new = emitter.BITCMB(psr_old, *src_value, fsxc_mask);
    emitter.STCPSR(psr_new);
  }

  emitter.STGPR(ir::GPR::PC, cpu_mode, emitter.LDCONST(r15 + 4u));
  return Code::Success;
}

TranslatorA32::Code TranslatorA32::Translate_LoadStoreMultiple(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter) {
  const u16 rlist = bit::get_field(instruction, 0u, 16u);
  const ir::GPR reg_base = bit::get_field<u32, ir::GPR>(instruction, 16u, 4u);

  const bool load = bit::get_bit(instruction, 20u);
  const bool writeback = bit::get_bit(instruction, 21u);
  const bool user_mode = bit::get_bit(instruction, 22u);
  const bool add = bit::get_bit(instruction, 23u);
  const bool pre_increment = bit::get_bit(instruction, 24u);

  if(rlist == 0u) {
    ATOM_PANIC("unhandled LDM/STM with empty rlist");
  }

  if(m_cpu_model == CPU::Model::ARM11 && bit::get_bit(rlist, (int)reg_base)) {
    ATOM_PANIC("unhandled ARM11 LDM/STM with base register in rlist");
  }

  // TODO(fleroviux): simplify all the edge cases around writeback...?

  const bool base_is_first = (rlist & ((1u << (int)reg_base) - 1u)) == 0u;
  const bool base_is_last = (rlist >> (int)reg_base) == 1u;
  const u32 bytes_transferred = std::popcount(rlist) * sizeof(u32);

  // TODO(fleroviux): this can be simplified I think
  const ir::U32Value* lo_address;
  const ir::U32Value* hi_address;
  const ir::U32Value* writeback_address;
  if(add) {
    lo_address = &emitter.LDGPR(reg_base, cpu_mode);
    hi_address = &emitter.ADD(*lo_address, emitter.LDCONST(bytes_transferred));
    writeback_address = hi_address;
  } else {
    hi_address = &emitter.LDGPR(reg_base, cpu_mode);
    lo_address = &emitter.SUB(*hi_address, emitter.LDCONST(bytes_transferred));
    writeback_address = lo_address;
  }

  AdvancePC(emitter, r15);

  const bool loading_r15 = load && bit::get_bit(rlist, 15u);
  const ir::Mode transfer_cpu_mode = (user_mode && !loading_r15) ? ir::Mode::User : cpu_mode;

  // STM ARMv4: store new base unless it is the first register
  // STM ARMv5: always store old base.
  const bool early_writeback = m_cpu_model == CPU::Model::ARM7 && writeback && !load && !base_is_first;
  if(early_writeback) {
    emitter.STGPR(reg_base, cpu_mode, *writeback_address);
  }

  const ir::U32Value* address = lo_address;

  for(int reg = 0; reg <= 15; reg++) {
    if(!bit::get_bit(rlist, reg)) {
      continue;
    }

    if(pre_increment == add) {
      address = &emitter.ADD(*address, emitter.LDCONST(sizeof(u32)));
    }

    if(load) {
      emitter.STGPR((ir::GPR)reg, transfer_cpu_mode, emitter.LDR(*address));
    } else {
      emitter.STR(*address, emitter.LDGPR((ir::GPR)reg, transfer_cpu_mode));
    }

    if(pre_increment != add) {
      address = &emitter.ADD(*address, emitter.LDCONST(sizeof(u32)));
    }
  }

  if(user_mode && loading_r15) {
    // TODO: base writeback happens in which mode? (this is unpredictable)
    // If writeback happens in the new mode, then this might be difficult
    // to emulate because we cannot know the value of SPSR at compile-time.
    emitter.STCPSR(emitter.LDSPSR(cpu_mode));
  }

  if(writeback) {
    if(load) {
      switch(m_cpu_model) {
        case CPU::Model::ARM7: {
          // LDM ARMv4: write back if base in not in the register list.
          if(!bit::get_bit(rlist, (int)reg_base)) {
            emitter.STGPR(reg_base, cpu_mode, *writeback_address);
          }
          break;
        }
        case CPU::Model::ARM9: {
          // LDM ARMv5: write back if base is the only register or not the last register.
          if(!base_is_last || rlist == (1u << (int)reg_base)) {
            emitter.STGPR(reg_base, cpu_mode, *writeback_address);
          }
          break;
        }
        default: {
          emitter.STGPR(reg_base, cpu_mode, *writeback_address);
          break;
        }
      }
    } else if(!early_writeback) {
      emitter.STGPR(reg_base, cpu_mode, *writeback_address);
    }
  }

  // Flush the pipeline if we loaded R15
  if(loading_r15) {
    if(user_mode) {
      // CPSR has changed, we can't assume to still be in ARM mode.
      // The code below is equivalent to `R15 = R15 + 8 - CPSR.thumb_bit * 4`:
      const ir::U32Value& offset_value = emitter.SUB(
        emitter.AND(
          emitter.LSR(emitter.LDCPSR(), emitter.LDCONST(3u)),
          emitter.LDCONST(4u)
        ),
        emitter.LDCONST(8u)
      );

      const ir::U32Value& old_pc_value = emitter.LDGPR(ir::GPR::PC, ir::Mode::System);
      const ir::U32Value& new_pc_value = emitter.SUB(old_pc_value, offset_value);
      emitter.STGPR(ir::GPR::PC, ir::Mode::System, new_pc_value);
    } else if(m_cpu_model != CPU::Model::ARM7) {
      FlushExchange(emitter, emitter.LDGPR(ir::GPR::PC, ir::Mode::System));
    } else {
      Flush(emitter, emitter.LDGPR(ir::GPR::PC, ir::Mode::System));
    }
  }

  return Code::Success;
}

TranslatorA32::Code TranslatorA32::Translate_Unimplemented(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter) {
  return Code::Fallback;
}

void TranslatorA32::AdvancePC(ir::Emitter& emitter, u32 current_r15) {
  emitter.STGPR(ir::GPR::PC, ir::Mode::System, emitter.LDCONST(current_r15 + sizeof(u32)));
}

void TranslatorA32::FlushExchange(ir::Emitter& emitter, const ir::U32Value& new_pc_value) {
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

void TranslatorA32::Flush(ir::Emitter& emitter, const ir::U32Value& new_pc_value) {
  emitter.STGPR(ir::GPR::PC, ir::Mode::System, emitter.ADD(new_pc_value, emitter.LDCONST(8u)));
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

  DECODE("cccc00010x00xxxxxxxxxxxx0000xxxx", MoveStatusRegToReg) // Move status register to register
  DECODE("cccc00010x10xxxxxxxxxxxx0000xxxx", MoveRegOrImmToStatusReg) // Move register to status register
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
  DECODE("cccc00110x10xxxxxxxxxxxxxxxxxxxx", MoveRegOrImmToStatusReg) // Move immediate to status register
  DECODE("cccc001xxxxxxxxxxxxxxxxxxxxxxxxx", DataProcessing) // Data Processing (Immediate)
  DECODE("cccc010xxxxxxxxxxxxxxxxxxxxxxxxx", Unimplemented)  // Load/Store (Immediate Offset)
  DECODE("cccc011xxxxxxxxxxxxxxxxxxxx0xxxx", Unimplemented)  // Load/Store (Register Offset)
  DECODE("cccc011xxxxxxxxxxxxxxxxxxxx1xxxx", Unimplemented)  // Undefined instruction
  DECODE("cccc100xxxxxxxxxxxxxxxxxxxxxxxxx", LoadStoreMultiple)  // Load/store multiple
  DECODE("cccc101xxxxxxxxxxxxxxxxxxxxxxxxx", Unimplemented)  // Branch and branch with link
  DECODE("cccc110xxxxxxxxxxxxxxxxxxxxxxxxx", Unimplemented)  // Coprocessor load/store and double register transfers
  DECODE("cccc1110xxxxxxxxxxxxxxxxxxx0xxxx", Unimplemented)  // Coprocessor data processing
  DECODE("cccc1110xxxxxxxxxxxxxxxxxxx1xxxx", Unimplemented)  // Coprocessor register transfers
  DECODE("cccc1111xxxxxxxxxxxxxxxxxxxxxxxx", Unimplemented)  // Software interrupt

  #undef DECODE

  return &TranslatorA32::Translate_Unimplemented;
}

} // namespace dual::arm::jit

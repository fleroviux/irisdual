
#include <atom/bit.hpp>

#include "translator_t16.hpp"

namespace bit = atom::bit;

namespace dual::arm::jit {

TranslatorT16::TranslatorT16() {
  BuildLUT();
}

TranslatorT16::Code TranslatorT16::Translate(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  const size_t hash = instruction >> 8;
  return (this->*m_handler_lut[hash])(r15, cpu_mode, instruction, emitter);
}

TranslatorT16::Code TranslatorT16::Translate_ShiftByImmediate(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  const ir::GPR reg_dst = bit::get_field<u16, ir::GPR>(instruction, 0u, 3u);
  const ir::GPR reg_src = bit::get_field<u16, ir::GPR>(instruction, 3u, 3u);
  const int imm_shift = bit::get_field<u16, int>(instruction, 6u, 5u);
  const Shift shift_op = bit::get_field<u16, Shift>(instruction, 11u, 2u);

  const ir::U32Value& src_value = emitter.LDGPR(reg_src, cpu_mode);
  const ir::U32Value* result_value = &src_value;

  // TODO(fleroviux): turn this into a reusable function
  const ir::HostFlagsValue* carry_out_value;
  switch(shift_op) {
    case Shift::LSL: {
      if(imm_shift != 0) {
        result_value = &emitter.LSL(src_value, emitter.LDCONST((u32)imm_shift), &carry_out_value);
        UpdateFlags(emitter, Flags::C, *carry_out_value);
      }
      break;
    }
    case Shift::LSR: {
      // LSR #32 is encoded as LSR #0
      if(imm_shift == 0) {
        ATOM_PANIC("unhandled LSR #32");
      }
      result_value = &emitter.LSR(src_value, emitter.LDCONST((u32)imm_shift), &carry_out_value);
      UpdateFlags(emitter, Flags::C, *carry_out_value);
      break;
    }
    case Shift::ASR: {
      // ASR #32 is encoded as ASR #0
      if(imm_shift == 0) {
        ATOM_PANIC("unhandled ASR #32");
      }
      result_value = &emitter.ASR(src_value, emitter.LDCONST((u32)imm_shift), &carry_out_value);
      UpdateFlags(emitter, Flags::C, *carry_out_value);
      break;
    }
    default: {
      // shift_op == 0b11 is used to encode ""Add/subtract register or immediate" instructions
      ATOM_UNREACHABLE()
    }
  }

  // Update NZ flags
  const ir::HostFlagsValue* hflag_value;
  emitter.AND(*result_value, *result_value, &hflag_value);
  UpdateFlags(emitter, Flags::NZ, *hflag_value);

  // Store the result in the destination register
  emitter.STGPR(reg_dst, cpu_mode, *result_value);

  AdvancePC(emitter, r15);
  return Code::Success;
}

TranslatorT16::Code TranslatorT16::Translate_AddSubtract(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  const ir::GPR reg_dst = bit::get_field<u16, ir::GPR>(instruction, 0u, 3u);
  const ir::GPR reg_lhs = bit::get_field<u16, ir::GPR>(instruction, 3u, 3u);
  const int opcode = bit::get_bit(instruction, 9u);
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
    case 0u: result_value = &emitter.ADD(lhs_value, *rhs_value, &hflag_value); break;
    case 1u: result_value = &emitter.SUB(lhs_value, *rhs_value, &hflag_value); break;
    default: ATOM_UNREACHABLE();
  }

  UpdateFlags(emitter, Flags::NZCV, *hflag_value);
  emitter.STGPR(reg_dst, cpu_mode, *result_value);

  AdvancePC(emitter, r15);
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
  DECODE("000ooiiiiimmmddd", ShiftByImmediate) // Shift by immediate
  DECODE("001oonnniiiiiiii", Unimplemented) // Add/subtract/compare/move immediate, NOTE: nnn is nnn and/or ddd
  DECODE("010000oooosssddd", Unimplemented) // Data-processing register, NOTE: sss = Rm/Rs, ddd = Rd/Rn
  DECODE("01000111LYmmmddd", Unimplemented) // Branch/exchange instruction set, Y = H2
  DECODE("010001ooXYmmmddd", Unimplemented) // Special data processing, X=H1, Y=H2, ddd = Rd/Rn
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

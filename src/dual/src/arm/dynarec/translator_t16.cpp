
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

TranslatorT16::Code TranslatorT16::Translate_Unimplemented(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter) {
  return Code::Fallback;
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
  DECODE("000110ommmnnnddd", Unimplemented) // Add/subtract register
  DECODE("000111oiiinnnddd", Unimplemented) // Add/subtract immediate
  DECODE("000ooiiiiimmmddd", Unimplemented) // Shift by immediate
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

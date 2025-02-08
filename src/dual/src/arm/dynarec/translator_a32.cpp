
#include <atom/bit.hpp>

#include "translator_a32.hpp"

namespace dual::arm::jit {

TranslatorA32::TranslatorA32() {
  BuildLUT();
}

TranslatorA32::Code TranslatorA32::Translate(u32 r15, CPU::Mode cpu_mode, u32 instruction, ir::Emitter& emitter) {
  using namespace ir;

  // Bail out on unconditional instructions for now, those need to be handled with special care
  if((instruction >> 28) == 15) {
    return Code::Fallback;
  }

  // Also bail out on conditionally executed instructions
  if((instruction >> 28) != 14) {
    return Code::Fallback;
  }

  const int hash = (instruction >> 16) & 0xFF0u | (instruction >> 4) & 0xFu;
  return m_handler_lut[hash](*this, instruction);
}

void TranslatorA32::BuildLUT() {
  for(u32 hash = 0u; hash < 0x1000; hash++) {
    const u32 instruction = (hash & 0xFF0u) << 16 | (hash & 0xFu) << 4;
    m_handler_lut[hash] = GetInstructionHandler(instruction);
  }
}

TranslatorA32::HandlerFn TranslatorA32::GetInstructionHandler(u32 instruction) {
  namespace bit = atom::bit;

  const auto Unimplemented = [](TranslatorA32& self, u32 instruction) {
    return Code::Fallback;
  };

  if(bit::match_pattern<"cccc00010x00xxxxxxxxxxxx0000xxxx">(instruction)) return Unimplemented; // Move status register to register
  if(bit::match_pattern<"cccc00010x10xxxxxxxxxxxx0000xxxx">(instruction)) return Unimplemented; // Move register to status register
  if(bit::match_pattern<"cccc00010xx0xxxxxxxxxxxx1xx0xxxx">(instruction)) return Unimplemented; // Enhanced DSP multiplies
  if(bit::match_pattern<"cccc000xxxxxxxxxxxxxxxxxxxx0xxxx">(instruction)) return Unimplemented; // Data Processing (Shift-by-Immediate)
  if(bit::match_pattern<"cccc00010010xxxxxxxxxxxx0001xxxx">(instruction)) return Unimplemented; // Branch/exchange instruction set
  if(bit::match_pattern<"cccc00010110xxxxxxxxxxxx0001xxxx">(instruction)) return Unimplemented; // Count leading zeros
  if(bit::match_pattern<"cccc00010010xxxxxxxxxxxx0011xxxx">(instruction)) return Unimplemented; // Branch and link/exchange instruction set
  if(bit::match_pattern<"cccc00010xx0xxxxxxxxxxxx0101xxxx">(instruction)) return Unimplemented; // Enhanced DSP add/subtracts
  if(bit::match_pattern<"cccc00010010xxxxxxxxxxxx0111xxxx">(instruction)) return Unimplemented; // Software breakpoint
  if(bit::match_pattern<"cccc000xxxxxxxxxxxxxxxxx0xx1xxxx">(instruction)) return Unimplemented; // Data Processing (Shift-by-Register)
  if(bit::match_pattern<"cccc000000xxxxxxxxxxxxxx1001xxxx">(instruction)) return Unimplemented; // Multiply (accumulate)
  if(bit::match_pattern<"cccc00001xxxxxxxxxxxxxxx1001xxxx">(instruction)) return Unimplemented; // Multiply (accumulate) long
  if(bit::match_pattern<"cccc00010x00xxxxxxxxxxxx1001xxxx">(instruction)) return Unimplemented;
  if(bit::match_pattern<"cccc000xx0xxxxxxxxxxxxxx1011xxxx">(instruction)) return Unimplemented; // Load/store halfword register offset
  if(bit::match_pattern<"cccc000xx1xxxxxxxxxxxxxx1011xxxx">(instruction)) return Unimplemented; // Load/store halfword immediate offset
  if(bit::match_pattern<"cccc000xx0x0xxxxxxxxxxxx11x1xxxx">(instruction)) return Unimplemented; // Load/store two words register offset
  if(bit::match_pattern<"cccc000xx0x1xxxxxxxxxxxx11x1xxxx">(instruction)) return Unimplemented; // Load signed halfword/byte register offset
  if(bit::match_pattern<"cccc000xx1x0xxxxxxxxxxxx11x1xxxx">(instruction)) return Unimplemented; // Load/store two words immediate offset
  if(bit::match_pattern<"cccc000xx1x1xxxxxxxxxxxx11x1xxxx">(instruction)) return Unimplemented; // Load signed halfword/byte register offset
  if(bit::match_pattern<"cccc00110x00xxxxxxxxxxxxxxxxxxxx">(instruction)) return Unimplemented;
  if(bit::match_pattern<"cccc00110x10xxxxxxxxxxxxxxxxxxxx">(instruction)) return Unimplemented; // Move immediate to status register
  if(bit::match_pattern<"cccc001xxxxxxxxxxxxxxxxxxxxxxxxx">(instruction)) return Unimplemented; // Data Processing (Immediate)
  if(bit::match_pattern<"cccc010xxxxxxxxxxxxxxxxxxxxxxxxx">(instruction)) return Unimplemented; // Load/Store (Immediate Offset)
  if(bit::match_pattern<"cccc011xxxxxxxxxxxxxxxxxxxx0xxxx">(instruction)) return Unimplemented; // Load/Store (Register Offset)
  if(bit::match_pattern<"cccc011xxxxxxxxxxxxxxxxxxxx1xxxx">(instruction)) return Unimplemented;
  if(bit::match_pattern<"cccc100xxxxxxxxxxxxxxxxxxxxxxxxx">(instruction)) return Unimplemented;
  if(bit::match_pattern<"cccc101xxxxxxxxxxxxxxxxxxxxxxxxx">(instruction)) return Unimplemented;
  if(bit::match_pattern<"cccc110xxxxxxxxxxxxxxxxxxxxxxxxx">(instruction)) return Unimplemented;
  if(bit::match_pattern<"cccc1110xxxxxxxxxxxxxxxxxxx0xxxx">(instruction)) return Unimplemented;
  if(bit::match_pattern<"cccc1110xxxxxxxxxxxxxxxxxxx1xxxx">(instruction)) return Unimplemented;
  if(bit::match_pattern<"cccc1111xxxxxxxxxxxxxxxxxxxxxxxx">(instruction)) return Unimplemented;

  return Unimplemented;
}

} // namespace dual::arm::jit

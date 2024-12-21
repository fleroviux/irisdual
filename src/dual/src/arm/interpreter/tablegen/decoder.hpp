
#pragma once

#include <atom/bit.hpp>
#include <atom/integer.hpp>

namespace dual::arm {

  /**
   * @todo:
   *   - decode remaining unconditional instructions (i.e.: PLD, MCR2, MRC2, ...)
   *   - split coprocessor load/store and double register transfer into two instruction types
   */

  enum class ARMInstrType {
    // Conditional
    HalfwordSignedTransfer,
    Multiply,
    SingleDataSwap,
    StatusTransfer,
    BranchAndExchange,
    CountLeadingZeros,
    BranchLinkExchange,
    SaturatingAddSubtract,
    Breakpoint,
    SignedHalfwordMultiply,
    DataProcessing,
    SingleDataTransfer,
    BlockDataTransfer,
    BranchAndLink,
    CoprocessorLoadStoreAndDoubleRegXfer,
    CoprocessorDataProcessing,
    CoprocessorRegisterXfer,
    SoftwareInterrupt,
    Undefined,

    // Unconditional
    PreLoadData,
    BranchLinkExchangeImm
  };

  enum class ThumbInstrType {
    MoveShiftedRegister,
    AddSub,
    MoveCompareAddSubImm,
    ALU,
    HighRegisterOps,
    LoadStoreRelativePC,
    LoadStoreOffsetReg,
    LoadStoreSigned,
    LoadStoreOffsetImm,
    LoadStoreHword,
    LoadStoreRelativeSP,
    LoadAddress,
    AddOffsetToSP,
    PushPop,
    SoftwareBreakpoint,
    LoadStoreMultiple,
    ConditionalBranch,
    SoftwareInterrupt,
    UnconditionalBranch,
    LongBranchLinkExchangeSuffix,
    LongBranchLinkPrefix,
    LongBranchLinkSuffix,
    Undefined
  };

  constexpr auto GetARMInstructionTypeUnconditional(u32 instruction) -> ARMInstrType {
    const auto opcode = instruction & 0x0FFFFFFF;

    switch(opcode >> 25) {
      case 0b010: return ARMInstrType::PreLoadData; // PLD #imm
      case 0b011: return ARMInstrType::PreLoadData; // PLD reg
      case 0b101: return ARMInstrType::BranchLinkExchangeImm;
    }

    return ARMInstrType::Undefined;
  }

  constexpr auto GetARMInstructionTypeConditional(u32 instruction) -> ARMInstrType {
    namespace bit = atom::bit;

    if(bit::match_pattern<"cccc00010x00xxxxxxxxxxxx0000xxxx">(instruction)) return ARMInstrType::StatusTransfer; // Move status register to register
    if(bit::match_pattern<"cccc00010x10xxxxxxxxxxxx0000xxxx">(instruction)) return ARMInstrType::StatusTransfer; // Move register to status register
    if(bit::match_pattern<"cccc00010xx0xxxxxxxxxxxx1xx0xxxx">(instruction)) return ARMInstrType::SignedHalfwordMultiply; // Enhanced DSP multiplies
    if(bit::match_pattern<"cccc000xxxxxxxxxxxxxxxxxxxx0xxxx">(instruction)) return ARMInstrType::DataProcessing; // Data Processing (Shift-by-Immediate)
    if(bit::match_pattern<"cccc00010010xxxxxxxxxxxx0001xxxx">(instruction)) return ARMInstrType::BranchAndExchange; // Branch/exchange instruction set
    if(bit::match_pattern<"cccc00010110xxxxxxxxxxxx0001xxxx">(instruction)) return ARMInstrType::CountLeadingZeros; // Count leading zeros
    if(bit::match_pattern<"cccc00010010xxxxxxxxxxxx0011xxxx">(instruction)) return ARMInstrType::BranchLinkExchange; // Branch and link/exchange instruction set
    if(bit::match_pattern<"cccc00010xx0xxxxxxxxxxxx0101xxxx">(instruction)) return ARMInstrType::SaturatingAddSubtract; // Enhanced DSP add/subtracts
    if(bit::match_pattern<"cccc00010010xxxxxxxxxxxx0111xxxx">(instruction)) return ARMInstrType::Breakpoint; // Software breakpoint
    if(bit::match_pattern<"cccc000xxxxxxxxxxxxxxxxx0xx1xxxx">(instruction)) return ARMInstrType::DataProcessing; // Data Processing (Shift-by-Register)
    if(bit::match_pattern<"cccc000000xxxxxxxxxxxxxx1001xxxx">(instruction)) return ARMInstrType::Multiply; // Multiply (accumulate)
    if(bit::match_pattern<"cccc00001xxxxxxxxxxxxxxx1001xxxx">(instruction)) return ARMInstrType::Multiply; // Multiply (accumulate) long
    if(bit::match_pattern<"cccc00010x00xxxxxxxxxxxx1001xxxx">(instruction)) return ARMInstrType::SingleDataSwap;
    if(bit::match_pattern<"cccc000xx0xxxxxxxxxxxxxx1011xxxx">(instruction)) return ARMInstrType::HalfwordSignedTransfer; // Load/store halfword register offset
    if(bit::match_pattern<"cccc000xx1xxxxxxxxxxxxxx1011xxxx">(instruction)) return ARMInstrType::HalfwordSignedTransfer; // Load/store halfword immediate offset
    if(bit::match_pattern<"cccc000xx0x0xxxxxxxxxxxx11x1xxxx">(instruction)) return ARMInstrType::HalfwordSignedTransfer; // Load/store two words register offset
    if(bit::match_pattern<"cccc000xx0x1xxxxxxxxxxxx11x1xxxx">(instruction)) return ARMInstrType::HalfwordSignedTransfer; // Load signed halfword/byte register offset
    if(bit::match_pattern<"cccc000xx1x0xxxxxxxxxxxx11x1xxxx">(instruction)) return ARMInstrType::HalfwordSignedTransfer; // Load/store two words immediate offset
    if(bit::match_pattern<"cccc000xx1x1xxxxxxxxxxxx11x1xxxx">(instruction)) return ARMInstrType::HalfwordSignedTransfer; // Load signed halfword/byte register offset
    if(bit::match_pattern<"cccc00110x00xxxxxxxxxxxxxxxxxxxx">(instruction)) return ARMInstrType::Undefined;
    if(bit::match_pattern<"cccc00110x10xxxxxxxxxxxxxxxxxxxx">(instruction)) return ARMInstrType::StatusTransfer; // Move immediate to status register
    if(bit::match_pattern<"cccc001xxxxxxxxxxxxxxxxxxxxxxxxx">(instruction)) return ARMInstrType::DataProcessing; // Data Processing (Immediate)
    if(bit::match_pattern<"cccc010xxxxxxxxxxxxxxxxxxxxxxxxx">(instruction)) return ARMInstrType::SingleDataTransfer; // Load/Store (Immediate Offset)
    if(bit::match_pattern<"cccc011xxxxxxxxxxxxxxxxxxxx0xxxx">(instruction)) return ARMInstrType::SingleDataTransfer; // Load/Store (Register Offset)
    if(bit::match_pattern<"cccc011xxxxxxxxxxxxxxxxxxxx1xxxx">(instruction)) return ARMInstrType::Undefined;
    if(bit::match_pattern<"cccc100xxxxxxxxxxxxxxxxxxxxxxxxx">(instruction)) return ARMInstrType::BlockDataTransfer;
    if(bit::match_pattern<"cccc101xxxxxxxxxxxxxxxxxxxxxxxxx">(instruction)) return ARMInstrType::BranchAndLink;
    if(bit::match_pattern<"cccc110xxxxxxxxxxxxxxxxxxxxxxxxx">(instruction)) return ARMInstrType::CoprocessorLoadStoreAndDoubleRegXfer;
    if(bit::match_pattern<"cccc1110xxxxxxxxxxxxxxxxxxx0xxxx">(instruction)) return ARMInstrType::CoprocessorDataProcessing;
    if(bit::match_pattern<"cccc1110xxxxxxxxxxxxxxxxxxx1xxxx">(instruction)) return ARMInstrType::CoprocessorRegisterXfer;
    if(bit::match_pattern<"cccc1111xxxxxxxxxxxxxxxxxxxxxxxx">(instruction)) return ARMInstrType::SoftwareInterrupt;

    return ARMInstrType::Undefined;
  }

  constexpr auto GetARMInstructionType(u32 instruction) -> ARMInstrType {
    const auto condition = instruction >> 28;
    if(condition == 15) {
      return GetARMInstructionTypeUnconditional(instruction);
    }
    return GetARMInstructionTypeConditional(instruction);
  }

  constexpr auto GetThumbInstructionType(u16 instruction) -> ThumbInstrType {
    namespace bit = atom::bit;

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

    if(bit::match_pattern<"000110ommmnnnddd">(instruction)) return ThumbInstrType::AddSub; // Add/subtract register
    if(bit::match_pattern<"000111oiiinnnddd">(instruction)) return ThumbInstrType::AddSub; // Add/subtract immediate
    if(bit::match_pattern<"000ooiiiiimmmddd">(instruction)) return ThumbInstrType::MoveShiftedRegister; // Shift by immediate
    if(bit::match_pattern<"001oonnniiiiiiii">(instruction)) return ThumbInstrType::MoveCompareAddSubImm; // Add/subtract/compare/move immediate, NOTE: nnn is nnn and/or ddd
    if(bit::match_pattern<"010000oooosssddd">(instruction)) return ThumbInstrType::ALU; // Data-processing register, NOTE: sss = Rm/Rs, ddd = Rd/Rn
    if(bit::match_pattern<"01000111LYmmmddd">(instruction)) return ThumbInstrType::HighRegisterOps; // Branch/exchange instruction set, Y = H2
    if(bit::match_pattern<"010001ooXYmmmddd">(instruction)) return ThumbInstrType::HighRegisterOps; // Special data processing, X=H1, Y=H2, ddd = Rd/Rn
    if(bit::match_pattern<"01001dddiiiiiiii">(instruction)) return ThumbInstrType::LoadStoreRelativePC; // Load from literal pool
    if(bit::match_pattern<"0101oo0mmmnnnddd">(instruction)) return ThumbInstrType::LoadStoreOffsetReg; // Load/store register offset
    if(bit::match_pattern<"0101oo1mmmnnnddd">(instruction)) return ThumbInstrType::LoadStoreSigned; // Load/store signed, FIXME: merge this with the format above
    if(bit::match_pattern<"011BLiiiiinnnddd">(instruction)) return ThumbInstrType::LoadStoreOffsetImm; // Load/store word/byte immediate offset
    if(bit::match_pattern<"1000Liiiiinnnddd">(instruction)) return ThumbInstrType::LoadStoreHword; // Load/store halfword immediate offset
    if(bit::match_pattern<"1001Ldddiiiiiiii">(instruction)) return ThumbInstrType::LoadStoreRelativeSP; // Load/store to/from stack
    if(bit::match_pattern<"1010Xdddiiiiiiii">(instruction)) return ThumbInstrType::LoadAddress; // Add to SP or PC, X = SP
    if(bit::match_pattern<"10110000oiiiiiii">(instruction)) return ThumbInstrType::AddOffsetToSP; // Adjust stack pointer
    if(bit::match_pattern<"1011L10Rrrrrrrrr">(instruction)) return ThumbInstrType::PushPop; // Push/pop register list
    if(bit::match_pattern<"10111110iiiiiiii">(instruction)) return ThumbInstrType::SoftwareBreakpoint; // Software breakpoint
    if(bit::match_pattern<"1100Lnnnrrrrrrrr">(instruction)) return ThumbInstrType::LoadStoreMultiple; // Load/store multiple
    if(bit::match_pattern<"11011110xxxxxxxx">(instruction)) return ThumbInstrType::Undefined; // Undefined instruction
    if(bit::match_pattern<"11011111iiiiiiii">(instruction)) return ThumbInstrType::SoftwareInterrupt; // Software interrupt
    if(bit::match_pattern<"1101cccciiiiiiii">(instruction)) return ThumbInstrType::ConditionalBranch; // Conditional branch
    if(bit::match_pattern<"11100iiiiiiiiiii">(instruction)) return ThumbInstrType::UnconditionalBranch; // Unconditional branch
    if(bit::match_pattern<"11101xxxxxxxxxx1">(instruction)) return ThumbInstrType::Undefined; // Undefined instruction
    if(bit::match_pattern<"11101iiiiiiiiiii">(instruction)) return ThumbInstrType::LongBranchLinkExchangeSuffix; // BLX suffix
    if(bit::match_pattern<"11110iiiiiiiiiii">(instruction)) return ThumbInstrType::LongBranchLinkPrefix; // BL/BLX prefix
    if(bit::match_pattern<"11111iiiiiiiiiii">(instruction)) return ThumbInstrType::LongBranchLinkSuffix; // BL suffix

    return ThumbInstrType::Undefined;
  }

} // namespace dual::arm

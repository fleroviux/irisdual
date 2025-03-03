
#pragma once

#include <atom/integer.hpp>
#include <array>

#include "ir/emitter.hpp"

namespace dual::arm::jit {

class TranslatorA32 {
  public:
    enum class Code {
      Success,
      Fallback
    };

    TranslatorA32();

    Code Translate(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);

  private:
    using HandlerFn = Code (TranslatorA32::*)(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);

    struct Flags {
      enum {
        N = 0x80000000,
        Z = 0x40000000,
        C = 0x20000000,
        V = 0x10000000,

        NZ = N | Z,
        NZC = N | Z | C,
        NZCV = N | Z | C | V
      };
    };

    enum class Shift {
      LSL = 0,
      LSR = 1,
      ASR = 2,
      ROR = 3
    };

    enum class DataOp {
      AND = 0,
      EOR = 1,
      SUB = 2,
      RSB = 3,
      ADD = 4,
      ADC = 5,
      SBC = 6,
      RSC = 7,
      TST = 8,
      TEQ = 9,
      CMP = 10,
      CMN = 11,
      ORR = 12,
      MOV = 13,
      BIC = 14,
      MVN = 15
    };

    Code Translate_DataProcessing(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);
    Code Translate_MRS(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);
    Code Translate_MSR_reg(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);
    Code Translate_MSR_imm(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);
    Code Translate_Unimplemented(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);

    static void UpdateFlags(ir::Emitter& emitter, u32 flag_set, const ir::HostFlagsValue& hflag_value);
    static void UpdateFlags(ir::Emitter& emitter, u32 flag_set, const ir::U32Value& nzcv_value);

    void BuildLUT();

    static HandlerFn GetInstructionHandler(u32 instruction);

    std::array<HandlerFn, 4096u> m_handler_lut{};
};

} // namespace dual::arm::jit

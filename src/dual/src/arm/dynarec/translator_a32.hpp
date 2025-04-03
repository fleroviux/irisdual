
#pragma once

#include <dual/arm/cpu.hpp>
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

    explicit TranslatorA32(CPU::Model cpu_model);

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
    Code Translate_MoveStatusRegToReg(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);
    Code Translate_MoveRegOrImmToStatusReg(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);
    Code Translate_LoadStoreMultiple(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);
    Code Translate_Unimplemented(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);

    void AdvancePC(ir::Emitter& emitter, u32 current_r15);
    void FlushExchange(ir::Emitter& emitter, const ir::U32Value& new_pc_value);
    void Flush(ir::Emitter& emitter, const ir::U32Value& new_pc_value);
    void UpdateFlags(ir::Emitter& emitter, u32 flag_set, const ir::HostFlagsValue& hflag_value);
    void UpdateFlags(ir::Emitter& emitter, u32 flag_set, const ir::U32Value& nzcv_value);

    void BuildLUT();

    static HandlerFn GetInstructionHandler(u32 instruction);

    // TODO(fleroviux): make this array static
    std::array<HandlerFn, 4096u> m_handler_lut{};

    CPU::Model m_cpu_model;
};

} // namespace dual::arm::jit


#pragma once

#include <atom/integer.hpp>
#include <array>

#include "ir/emitter.hpp"

namespace dual::arm::jit {

class TranslatorT16 {
  public:
    enum class Code {
      Success,
      Fallback
    };

    TranslatorT16();

    Code Translate(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);

    Code Translate_ShiftByImm(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);
    Code Translate_AddSubtract(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);
    Code Translate_AddSubCmpMovImm(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);
    Code Translate_DataProcessingReg(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);
    Code Translate_SpecialDataProcessing(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);
    Code Translate_LoadFromLiteralPool(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);
    Code Translate_LoadStoreRegOffset(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);
    Code Translate_LoadStoreWordByteImmOffset(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);
    Code Translate_Unimplemented(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);

  private:
    using HandlerFn = Code (TranslatorT16::*)(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);

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

    void AdvancePC(ir::Emitter& emitter, u32 current_r15);
    void UpdateFlags(ir::Emitter& emitter, u32 flag_set, const ir::HostFlagsValue& hflag_value);
    void UpdateFlags(ir::Emitter& emitter, u32 flag_set, const ir::U32Value& nzcv_value);

    void BuildLUT();

    static HandlerFn GetInstructionHandler(u16 instruction);

    // TODO(fleroviux): make this array static
    std::array<HandlerFn, 256u> m_handler_lut{};
};

} // namespace dual::arm::jit

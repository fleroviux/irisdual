
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

    Code Translate_MRS(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);
    Code Translate_MSR_reg(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);
    Code Translate_MSR_imm(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);
    Code Translate_Unimplemented(u32 r15, ir::Mode cpu_mode, u32 instruction, ir::Emitter& emitter);

    static HandlerFn GetInstructionHandler(u32 instruction);

    void BuildLUT();

    std::array<HandlerFn, 4096u> m_handler_lut{};
};

} // namespace dual::arm::jit

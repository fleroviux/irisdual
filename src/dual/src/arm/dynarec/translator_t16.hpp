
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

    Code Translate_Unimplemented(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);

  private:
    using HandlerFn = Code (TranslatorT16::*)(u32 r15, ir::Mode cpu_mode, u16 instruction, ir::Emitter& emitter);

    void BuildLUT();

    static HandlerFn GetInstructionHandler(u16 instruction);

    // TODO(fleroviux): make this array static
    std::array<HandlerFn, 256u> m_handler_lut{};
};

} // namespace dual::arm::jit

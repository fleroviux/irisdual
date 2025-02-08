
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

    // TODO: make some of this stuff static and constexpr?
    using HandlerFn = Code (*)(TranslatorA32& self, u32 instruction);
    void BuildLUT();
    HandlerFn GetInstructionHandler(u32 instruction);
    std::array<HandlerFn, 4096u> m_handler_lut{};
};

} // namespace dual::arm::jit

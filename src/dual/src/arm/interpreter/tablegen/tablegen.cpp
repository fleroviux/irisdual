
#include <atom/meta.hpp>

#include "arm/interpreter/interpreter_cpu.hpp"
#include "decoder.hpp"

namespace dual::arm {

  using Handler16 = InterpreterCPU::Handler16;
  using Handler32 = InterpreterCPU::Handler32;

  /** A helper class used to generate lookup tables for
    * the interpreter at compiletime.
    * The motivation is to separate the code used for generation from
    * the interpreter class and its header itself.
    */
  struct TableGen {
    #ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Weverything"
    #endif

    #include "gen_arm.hpp"
    #include "gen_thumb.hpp"

    #ifdef __clang__
    #pragma clang diagnostic pop
    #endif

    static constexpr auto GenerateTableThumb() -> std::array<Handler16, 2048> {
      std::array<Handler16, 2048> lut = {};

      atom::static_for<std::size_t, 0, 2048>([&](auto i) {
        lut[i] = GenerateHandlerThumb<i << 5>();
      });
      return lut;
    }

    static constexpr auto GenerateTableARM() -> std::array<Handler32, 8192> {
      std::array<Handler32, 8192> lut = {};

      // Conditional instructions
      atom::static_for<std::size_t, 0, 4096>([&](auto i) {
        lut[i] = GenerateHandlerARM<
          ((i & 0xFF0) << 16) |
          ((i & 0xF) << 4)>();
      });

      // Unconditional instructions
      atom::static_for<std::size_t, 0, 4096>([&](auto i) {
        lut[4096 + i] = GenerateHandlerARM<
          ((i & 0xFF0) << 16) |
          ((i & 0xF) << 4) | 0xF0000000>();
      });

      return lut;
    }
  };

  std::array<Handler16, 2048> InterpreterCPU::k_opcode_lut_16 = TableGen::GenerateTableThumb();
  std::array<Handler32, 8192> InterpreterCPU::k_opcode_lut_32 = TableGen::GenerateTableARM();

} // namespace dual::arm


#pragma once

#include <atom/integer.hpp>
#include <array>

#include "operand.hpp"

namespace dual::arm::jit::a64mir {

struct Instruction {
  enum class Type : u16 {
    LDGPR
  };

  static constexpr size_t max_arg_slots = 2u;
  static constexpr size_t max_out_slots = 2u;

  Instruction(Type type, u8 arg_slot_count, u8 out_slot_count)
      : type{type}
      , arg_slot_count{arg_slot_count}
      , out_slot_count{out_slot_count} {
  }

  Type type;
  u8 arg_slot_count;
  u8 out_slot_count;
  std::array<Operand, max_arg_slots> arg_slots{};
  std::array<Operand, max_out_slots> out_slots{};
  Instruction* prev{};
  Instruction* next{};

  static_assert(max_arg_slots <= std::numeric_limits<decltype(arg_slot_count)>::max());
  static_assert(max_out_slots <= std::numeric_limits<decltype(out_slot_count)>::max());
};

} // namespace dual::arm::jit::a64mir

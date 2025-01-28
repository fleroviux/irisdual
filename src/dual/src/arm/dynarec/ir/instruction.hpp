
#pragma once

#include <atom/integer.hpp>
#include <array>
#include <limits>

#include "input.hpp"
#include "value.hpp"

namespace dual::arm::jit::ir {

struct Instruction {
  enum class Type : u16 {
    // Guest Context Read/Write
    LDGPR,
    STGPR,
    LDCPSR,
    STCPSR,

    // Flag Management
    // @todo: CVT_NZCV2HFLAG operation might be wasteful, we will probably only need the carry usually.
    CVT_HFLAG2NZCV,
    CVT_HFLAG2Q,
    CVT_NZCV2HFLAG,

    // Binary Ops
    ADD,
  };

  enum class Flags : u16 {
  };

  static constexpr size_t max_arg_slots = 2u;
  static constexpr size_t max_ret_slots = 2u; // @todo: make naming consistent

  Instruction(Type type, u16 flags, u8 arg_slot_count, u8 ret_slot_count)
      : type{type}
      , flags{flags}
      , arg_slot_count{arg_slot_count}
      , ret_slot_count{ret_slot_count} {
    arg_slots.fill({});
    ret_slots.fill(Value::invalid_id);
  }

  [[nodiscard]] const Input& GetArg(size_t arg_index) const {
    return arg_slots[arg_index];
  }

  [[nodiscard]] Value::ID GetOut(size_t out_index) const {
    return ret_slots[out_index];
  }

  Type type;
  u16 flags;
  u8 arg_slot_count;
  u8 ret_slot_count;
  std::array<Input,     max_arg_slots> arg_slots{};
  std::array<Value::ID, max_ret_slots> ret_slots{};
  Instruction* prev{};
  Instruction* next{};

  static_assert(max_arg_slots <= std::numeric_limits<decltype(arg_slot_count)>::max());
  static_assert(max_ret_slots <= std::numeric_limits<decltype(ret_slot_count)>::max());
};

} // namespace dual::arm::jit::ir

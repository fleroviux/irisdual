
#pragma once

#include <atom/integer.hpp>
#include <array>
#include <limits>

#include "input.hpp"
#include "value.hpp"

namespace dual::arm::jit::ir {

struct Instruction {
  enum class Type : u16 {
    // Constant Loading
    LDCONST,

    // Guest State Read/Write
    LDGPR,
    STGPR,
    LDCPSR,
    STCPSR,
    LDSPSR,
    STSPSR,

    // Flag Management
    CVT_HFLAG_NZCV,
    // ...

    // Control Flow
    BR,
    BR_IF,
    EXIT,

    // Barrel Shifter
    LSL,
    LSR,
    ASR,
    ROR,
    RRX,

    // Data processing
    AND,
    BIC,
    EOR,
    SUB,
    RSB,
    ADD,
    ADC,
    SBC,
    RSC,
    ORR,
    MOV,
    MVN,
  };

  enum Flag : u16 {
    OutputHostFlags = 1 << 0
  };

  static constexpr size_t max_arg_slots = 4u;
  static constexpr size_t max_out_slots = 2u;

  Instruction(Type type, u16 flags, u8 arg_slot_count, u8 out_slot_count)
      : type{type}
      , flags{flags}
      , arg_slot_count{arg_slot_count}
      , out_slot_count{out_slot_count} {
    arg_slots.fill({});
    out_slots.fill(Value::invalid_id);
  }

  [[nodiscard]] const Input& GetArg(size_t arg_index) const {
    return arg_slots[arg_index];
  }

  [[nodiscard]] Value::ID GetOut(size_t out_index) const {
    return out_slots[out_index];
  }

  Type type;
  u16 flags;
  u8 arg_slot_count;
  u8 out_slot_count;
  std::array<Input,     max_arg_slots> arg_slots{};
  std::array<Value::ID, max_out_slots> out_slots{};
  Instruction* prev{};
  Instruction* next{};

  static_assert(max_arg_slots <= std::numeric_limits<decltype(arg_slot_count)>::max());
  static_assert(max_out_slots <= std::numeric_limits<decltype(out_slot_count)>::max());
};

} // namespace dual::arm::jit::ir

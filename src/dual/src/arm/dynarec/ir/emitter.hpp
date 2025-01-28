
#pragma once

#include <atom/arena.hpp>
#include <atom/integer.hpp>
#include <atom/panic.hpp>
#include <tuple>
#include <type_traits>
#include <utility>

#include "basic_block.hpp"
#include "value.hpp"
#include "input.hpp"
#include "instruction.hpp"

namespace dual::arm::jit::ir {

class Emitter {
  public:
    Emitter(BasicBlock& basic_block, atom::Arena& arena)
        : m_basic_block{basic_block}
        , m_arena{arena} {
    }

    const U32Value& Const(u32 imm_u32) {
      auto& value = CreateValue<U32Value>();
      value.create_ref.imm_u64 = imm_u32;
      return value;
    }

    const I32Value& Const(i32 imm_i32) {
      auto& value = CreateValue<I32Value>();
      value.create_ref.imm_i64 = imm_i32;
      return value;
    }

    const U32Value& LDGPR(GPR gpr) {
      return std::get<0>(Emit<U32Value>(Type::LDGPR, 0u, gpr));
    }

    void STGPR(GPR gpr, const U32Value& value) {
      Emit(Instruction::Type::STGPR, 0u, gpr, value);
    }

    const U32Value& LDCPSR() {
      return std::get<0>(Emit<U32Value>(Type::LDCPSR, 0u));
    }

    void STCPSR(const U32Value& value) {
      Emit(Type::STCPSR, 0u, value);
    }

    const U32Value& ADD(const U32Value& lhs, const U32Value& rhs, const HostFlagsValue** hflags_out = nullptr) {
      if(hflags_out) {
        const auto [result_value, hflags_value] = Emit<U32Value, HostFlagsValue>(Type::ADD, Flag::OutputHostFlags, lhs, rhs);
        *hflags_out = &hflags_value;
        return result_value;
      }
      return std::get<0>(Emit<U32Value>(Type::ADD, 0u, lhs, rhs));
    }

  private:
    using Type = Instruction::Type;
    using Flag = Instruction::Flag;

    template<typename T>
    using ConstRef = std::add_lvalue_reference_t<std::add_const_t<T>>;

    template<typename... ResultTypes, typename... ArgumentTypes>
    std::tuple<ConstRef<ResultTypes>...> Emit(Type type, u16 flags, ArgumentTypes&&... args) {
      static_assert(sizeof...(ResultTypes)   <= Instruction::max_ret_slots);
      static_assert(sizeof...(ArgumentTypes) <= Instruction::max_arg_slots);

      Instruction& instruction = AppendInstruction(type, flags, sizeof...(ArgumentTypes), sizeof...(ResultTypes));

      if constexpr(sizeof...(ArgumentTypes) != 0u) {
        SetArguments(instruction, 0, std::forward<ArgumentTypes>(args)...);
      }

      if constexpr(sizeof...(ResultTypes) != 0u) {
        return CreateResultValues<ResultTypes...>(instruction, 0);
      } else {
        return {};
      }
    }

    Instruction& AppendInstruction(Type type, u16 flags, size_t arg_slot_count, size_t ret_slot_count) {
      const auto instruction = (Instruction*)m_arena.Allocate(sizeof(Instruction));
      if(instruction == nullptr) [[unlikely]] {
        ATOM_PANIC("ran out of memory arena space");
      }
      new(instruction) Instruction{type, flags, (u8)arg_slot_count, (u8)ret_slot_count};

      if(m_basic_block.head == nullptr) [[unlikely]] {
        m_basic_block.head = instruction;
        m_basic_block.tail = instruction;
      } else {
        m_basic_block.tail->next = instruction;
        instruction->prev = m_basic_block.tail;
        m_basic_block.tail = instruction;
      }

      return *instruction;
    }

    template<typename ArgumentType, typename... RemainingArgumentTypes>
    static void SetArguments(Instruction& instruction, int first_slot, ArgumentType&& arg, RemainingArgumentTypes&&... args) {
      SetArgument(instruction, first_slot, std::forward<ArgumentType>(arg));
      if constexpr(sizeof...(RemainingArgumentTypes) != 0u) {
        SetArguments(instruction, first_slot + 1, std::forward<RemainingArgumentTypes>(args)...);
      }
    }

    static void SetArgument(Instruction& instruction, int slot, const Value& value) {
      instruction.arg_slots[slot] = Input{value};
      value.use_refs.push_back({.instruction = &instruction, .slot = slot});
    }

    static void SetArgument(Instruction& instruction, int slot, GPR gpr) {
      instruction.arg_slots[slot] = Input{gpr};
    }

    template<typename ResultType, typename... RemainingResultTypes>
    std::tuple<ConstRef<ResultType>, ConstRef<RemainingResultTypes>...> CreateResultValues(Instruction& instruction, int first_slot) {
      auto& value = CreateResultValue<ResultType>(instruction, first_slot);
      if constexpr(sizeof...(RemainingResultTypes) == 0u) {
        return std::tuple<const ResultType&>{value};
      } else {
        return std::tuple_cat(std::tuple<const ResultType&>{value}, CreateResultValues<RemainingResultTypes...>(instruction, first_slot + 1));
      }
    }

    template<typename ResultType>
    requires std::is_base_of_v<Value, ResultType>
    const ResultType& CreateResultValue(Instruction& instruction, int slot) {
      auto& value = CreateValue<ResultType>();
      value.create_ref.instruction = &instruction;
      value.create_ref.slot = slot;
      instruction.ret_slots[slot] = value.id;
      return value;
    }

    // @todo: move this to basic block?
    template<typename ValueType>
    requires std::is_base_of_v<Value, ValueType>
    ValueType& CreateValue() {
      const auto value = (ValueType*)m_arena.Allocate(sizeof(ValueType));
      if(value == nullptr) {
        ATOM_PANIC("ran out of memory arena space");
      }
      const Value::ID id = m_basic_block.values.size();
      if(id == Value::invalid_id) {
        ATOM_PANIC("exceeded maximum number of values per basic block limit");
      }
      new(value) ValueType{id};
      m_basic_block.values.push_back(value);
      return *value;
    }

    BasicBlock& m_basic_block;
    atom::Arena& m_arena;
};

} // namespace dual::arm::jit::ir

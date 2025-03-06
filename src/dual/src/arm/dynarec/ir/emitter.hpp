
#pragma once

#include <atom/arena.hpp>
#include <atom/integer.hpp>
#include <atom/panic.hpp>
#include <tuple>
#include <type_traits>
#include <utility>

#include "basic_block.hpp"
#include "instruction.hpp"

namespace dual::arm::jit::ir {

class Emitter {
  public:
    Emitter(BasicBlock& basic_block, atom::Arena& arena)
        : m_basic_block{basic_block}
        , m_arena{arena} {
    }

    const U32Value& LDCONST(u32 const_u32) {
      return std::get<0>(Emit<U32Value>(Type::LDCONST, 0u, const_u32));
    }

    const U32Value& LDGPR(GPR gpr, Mode cpu_mode) {
      return std::get<0>(Emit<U32Value>(Type::LDGPR, 0u, gpr, cpu_mode));
    }

    void STGPR(GPR gpr, Mode cpu_mode,  const U32Value& value) {
      Emit(Instruction::Type::STGPR, 0u, gpr, cpu_mode, value);
    }

    const U32Value& LDCPSR() {
      return std::get<0>(Emit<U32Value>(Type::LDCPSR, 0u));
    }

    void STCPSR(const U32Value& value) {
      Emit(Type::STCPSR, 0u, value);
    }

    const U32Value& LDSPSR(Mode cpu_mode) {
      return std::get<0>(Emit<U32Value>(Type::LDSPSR, 0u, cpu_mode));
    }

    void STSPSR(Mode cpu_mode, const U32Value& value) {
      Emit(Type::STSPSR, 0u, cpu_mode, value);
    }

    const U32Value& CVT_HFLAG_NZCV(const HostFlagsValue& hflag_value) {
      return std::get<0>(Emit<U32Value>(Type::CVT_HFLAG_NZCV, 0u, hflag_value));
    }

    const HostFlagsValue& CVT_NZCV_HFLAG(const U32Value& nzcv_value) {
      return std::get<0>(Emit<HostFlagsValue>(Type::CVT_NZCV_HFLAG, 0u, nzcv_value));
    }

    void BR(const BasicBlock& basic_block) {
      Emit(Type::BR, 0u, basic_block);
    }

    void BR_IF(Condition condition, const HostFlagsValue& hflag_value, const BasicBlock& bb_true, const BasicBlock& bb_false) {
      Emit(Type::BR_IF, 0u, condition, hflag_value, bb_true, bb_false);
    }

    void EXIT() {
      Emit(Type::EXIT, 0u);
    }

    const U32Value& LSL(const U32Value& value, const U32Value& shift_amount, const HostFlagsValue** carry_out = nullptr) {
      return EmitBinaryALU(Type::LSL, value, shift_amount, carry_out);
    }

    const U32Value& LSR(const U32Value& value, const U32Value& shift_amount, const HostFlagsValue** carry_out = nullptr) {
      return EmitBinaryALU(Type::LSR, value, shift_amount, carry_out);
    }

    const U32Value& ASR(const U32Value& value, const U32Value& shift_amount, const HostFlagsValue** carry_out = nullptr) {
      return EmitBinaryALU(Type::ASR, value, shift_amount, carry_out);
    }

    const U32Value& ROR(const U32Value& value, const U32Value& shift_amount, const HostFlagsValue** carry_out = nullptr) {
      return EmitBinaryALU(Type::ROR, value, shift_amount, carry_out);
    }

    const U32Value& RRX(const U32Value& value, const HostFlagsValue& carry_in, const HostFlagsValue** carry_out = nullptr) {
      if(carry_out) {
        const auto [result_value, hflags_value] = Emit<U32Value, HostFlagsValue>(Type::RRX, Flag::OutputHostFlags, value, carry_in);
        *carry_out = &hflags_value;
        return result_value;
      }
      return std::get<0>(Emit<U32Value>(Type::RRX, 0u, value, carry_in));
    }

    const U32Value& ADD(const U32Value& lhs, const U32Value& rhs, const HostFlagsValue** hflags_out = nullptr) {
      return EmitBinaryALU(Type::ADD, lhs, rhs, hflags_out);
    }

    const U32Value& ADC(const U32Value& lhs, const U32Value& rhs, const HostFlagsValue& carry_in, const HostFlagsValue** hflags_out = nullptr) {
      return EmitBinaryALUWithCarry(Type::ADC, lhs, rhs, carry_in, hflags_out);
    }

    const U32Value& SUB(const U32Value& lhs, const U32Value& rhs, const HostFlagsValue** hflags_out = nullptr) {
      return EmitBinaryALU(Type::SUB, lhs, rhs, hflags_out);
    }

    const U32Value& SBC(const U32Value& lhs, const U32Value& rhs, const HostFlagsValue& carry_in, const HostFlagsValue** hflags_out = nullptr) {
      return EmitBinaryALUWithCarry(Type::SBC, lhs, rhs, carry_in, hflags_out);
    }

    const U32Value& AND(const U32Value& lhs, const U32Value& rhs, const HostFlagsValue** hflags_out = nullptr) {
      return EmitBinaryALU(Type::AND, lhs, rhs, hflags_out);
    }

    const U32Value& BIC(const U32Value& lhs, const U32Value& rhs, const HostFlagsValue** hflags_out = nullptr) {
      return EmitBinaryALU(Type::BIC, lhs, rhs, hflags_out);
    }

    const U32Value& ORR(const U32Value& lhs, const U32Value& rhs, const HostFlagsValue** hflags_out = nullptr) {
      return EmitBinaryALU(Type::ORR, lhs, rhs, hflags_out);
    }

    const U32Value& EOR(const U32Value& lhs, const U32Value& rhs, const HostFlagsValue** hflags_out = nullptr) {
      return EmitBinaryALU(Type::EOR, lhs, rhs, hflags_out);
    }

    const U32Value& NOT(const U32Value& value) {
      return std::get<0>(Emit<U32Value>(Type::NOT, 0u, value));
    }

    const U32Value& MUL(const U32Value& lhs, const U32Value& rhs) {
      return std::get<0>(Emit<U32Value>(Type::MUL, 0u, lhs, rhs));
    }

    const U32Value& BITCMB(const U32Value& lhs, const U32Value& rhs, const U32Value& mask) {
      return std::get<0>(Emit<U32Value>(Type::BITCMB, 0u, lhs, rhs, mask));
    }

    const U32Value& BITCMB(const U32Value& lhs, const U32Value& rhs, u32 mask) {
      return BITCMB(lhs, rhs, LDCONST(mask));
    }

    const U32Value& CSEL(Condition condition, const HostFlagsValue& hflag_value, const U32Value& true_value, const U32Value& false_value) {
      return std::get<0>(Emit<U32Value>(Type::CSEL, 0u, condition, hflag_value, true_value, false_value));
    }

  private:
    using Type = Instruction::Type;
    using Flag = Instruction::Flag;

    const U32Value& EmitBinaryALU(Type type, const U32Value& lhs, const U32Value& rhs, const HostFlagsValue** hflags_out) {
      if(hflags_out) {
        const auto [result_value, hflags_value] = Emit<U32Value, HostFlagsValue>(type, Flag::OutputHostFlags, lhs, rhs);
        *hflags_out = &hflags_value;
        return result_value;
      }
      return std::get<0>(Emit<U32Value>(type, 0u, lhs, rhs));
    }

    const U32Value& EmitBinaryALUWithCarry(Type type, const U32Value& lhs, const U32Value& rhs, const HostFlagsValue& carry_in, const HostFlagsValue** hflags_out) {
      if(hflags_out) {
        const auto [result_value, hflags_value] = Emit<U32Value, HostFlagsValue>(type, Flag::OutputHostFlags, lhs, rhs, carry_in);
        *hflags_out = &hflags_value;
        return result_value;
      }
      return std::get<0>(Emit<U32Value>(type, 0u, lhs, rhs, carry_in));
    }

    template<typename T>
    using ConstRef = std::add_lvalue_reference_t<std::add_const_t<T>>;

    template<typename... OutTypes, typename... ArgTypes>
    std::tuple<ConstRef<OutTypes>...> Emit(Type type, u16 flags, ArgTypes&&... args) {
      static_assert(sizeof...(OutTypes)   <= Instruction::max_out_slots);
      static_assert(sizeof...(ArgTypes) <= Instruction::max_arg_slots);

      Instruction& instruction = AppendInstruction(type, flags, sizeof...(ArgTypes), sizeof...(OutTypes));

      if constexpr(sizeof...(ArgTypes) != 0u) {
        SetArgs(instruction, 0, std::forward<ArgTypes>(args)...);
      }

      if constexpr(sizeof...(OutTypes) != 0u) {
        return CreateOutValues<OutTypes...>(instruction, 0);
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

    template<typename ArgType, typename... RemainingArgTypes>
    static void SetArgs(Instruction& instruction, int first_slot, ArgType&& arg, RemainingArgTypes&&... args) {
      SetArg(instruction, first_slot, std::forward<ArgType>(arg));
      if constexpr(sizeof...(RemainingArgTypes) != 0u) {
        SetArgs(instruction, first_slot + 1, std::forward<RemainingArgTypes>(args)...);
      }
    }

    static void SetArg(Instruction& instruction, int slot, const Value& value) {
      instruction.arg_slots[slot] = Input{value};
      value.use_refs.push_back({.instruction = &instruction, .slot = slot});
    }

    static void SetArg(Instruction& instruction, int slot, GPR gpr) {
      instruction.arg_slots[slot] = Input{gpr};
    }

    static void SetArg(Instruction& instruction, int slot, Mode mode) {
      instruction.arg_slots[slot] = Input{mode};
    }

    static void SetArg(Instruction& instruction, int slot, u32 const_u32) {
      instruction.arg_slots[slot] = Input{const_u32};
    }

    static void SetArg(Instruction& instruction, int slot, Condition condition) {
      instruction.arg_slots[slot] = Input{condition};
    }

    static void SetArg(Instruction& instruction, int slot, const BasicBlock& basic_block) {
      instruction.arg_slots[slot] = Input{basic_block};
    }

    template<typename OutType, typename... RemainingOutTypes>
    std::tuple<ConstRef<OutType>, ConstRef<RemainingOutTypes>...> CreateOutValues(Instruction& instruction, int first_slot) {
      auto& value = CreateOutValue<OutType>(instruction, first_slot);
      if constexpr(sizeof...(RemainingOutTypes) == 0u) {
        return std::tuple<const OutType&>{value};
      } else {
        return std::tuple_cat(std::tuple<const OutType&>{value}, CreateOutValues<RemainingOutTypes...>(instruction, first_slot + 1));
      }
    }

    template<typename OutType>
    requires std::is_base_of_v<Value, OutType>
    const OutType& CreateOutValue(Instruction& instruction, int slot) {
      auto& value = CreateValue<OutType>();
      value.create_ref.instruction = &instruction;
      value.create_ref.slot = slot;
      instruction.out_slots[slot] = value.id;
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

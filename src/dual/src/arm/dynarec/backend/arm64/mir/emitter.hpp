
#pragma once

#include <atom/arena.hpp>
#include <atom/panic.hpp>

#include "basic_block.hpp"
#include "instruction.hpp"

namespace dual::arm::jit::a64mir {

class Emitter {
  public:
    Emitter(BasicBlock& basic_block, atom::Arena& arena)
        : m_basic_block{basic_block}
        , m_arena{arena} {
    }

//    void LDGPR(GPR gpr, Mode cpu_mode) {
//      Instruction& instruction = AppendInstruction(Type::LDGPR, 2u, 0u);
//      instruction.arg_slots[0] = Operand{gpr};
//      instruction.arg_slots[1] = Operand{cpu_mode};
//
//      const VReg& result = CreateVReg();
//      instruction.out_slots[0] = Operand{result};
//    }

    template<typename... AddressArgs>
    void LDR(AddressArgs&&... args) {
      Instruction& instruction = AppendInstruction(Type::LDR, 1u, 0u);
      instruction.arg_slots[0] = Operand{Address{std::forward<AddressArgs>(args)...}};
      // ...
    }

  private:
    using Type = Instruction::Type;

    Instruction& AppendInstruction(Type type, size_t arg_slot_count, size_t ret_slot_count) {
      const auto instruction = (Instruction*)m_arena.Allocate(sizeof(Instruction));
      if(instruction == nullptr) [[unlikely]] {
        ATOM_PANIC("ran out of memory arena space");
      }
      new(instruction) Instruction{type, (u8)arg_slot_count, (u8)ret_slot_count};

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

    // @todo: move this to basic block?
    const VReg& CreateVReg() {
      const auto vreg = (VReg*)m_arena.Allocate(sizeof(VReg));
      if(vreg == nullptr) {
        ATOM_PANIC("ran out of memory arena space");
      }
      const VReg::ID id = m_basic_block.vregs.size();
      if(id == std::numeric_limits<VReg::ID>::max()) {
        ATOM_PANIC("exceeded maximum number of values per basic block limit");
      }
      new(vreg) VReg{id};
      m_basic_block.vregs.push_back(vreg);
      return *vreg;
    }

    BasicBlock& m_basic_block;
    atom::Arena& m_arena;
};

} // namespace dual::arm::jit::a64mir


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

    BasicBlock& m_basic_block;
    atom::Arena& m_arena;
};

} // namespace dual::arm::jit::a64mir


#pragma once

#include <atom/panic.hpp>

#include "basic_block.hpp"
#include "function.hpp"
#include "instruction.hpp"
#include "value.hpp"

namespace dual::arm::jit::ir {

class Pass {
  public:
    virtual ~Pass() = default;

    virtual void Run(Function& function) = 0;

  protected:
    // TODO: these utility functions likely should go elsewhere in the long run.

    static void RewriteValueUseRefs(BasicBlock& basic_block, Value::ID old_value_id, Value::ID new_value_id) {
      if(old_value_id == new_value_id) {
        return;
      }

      Value* old_value = basic_block.values[old_value_id];
      Value* new_value = basic_block.values[new_value_id];

      for(const Ref& use_ref : old_value->use_refs) {
        use_ref.instruction->arg_slots[use_ref.slot] = Input{*new_value};
        new_value->use_refs.push_back(use_ref);
      }

      old_value->use_refs.clear();
    }

    static void RemoveInstruction(BasicBlock& basic_block, Instruction* instruction) {
      // Ensure that the instruction has no uses before it is deleted.
      // TODO: this is somewhat inefficient, only do this in debug?
      for(size_t out_slot = 0u; out_slot < instruction->out_slot_count; out_slot++) {
        Value* value = basic_block.values[instruction->out_slots[out_slot]];
        if(!value->use_refs.empty()) {
          ATOM_PANIC("IR instruction cannot be removed because it has active uses");
        }
      }

      if(instruction->prev != nullptr) {
        instruction->prev->next = instruction->next;
      } else {
        basic_block.head = instruction->next;
      }

      if(instruction->next != nullptr) {
        instruction->next->prev = instruction->prev;
      } else {
        basic_block.tail = instruction->prev;
      }
    }
};

} // namespace dual::arm::jit::ir

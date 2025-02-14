
#pragma once

#include <atom/integer.hpp>
#include <atom/panic.hpp>
#include <algorithm>
#include <optional>

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
      // Remove create ref from any values created by this instruction and ensure that the values have no uses.
      for(size_t out_slot = 0u; out_slot < instruction->out_slot_count; out_slot++) {
        Value* value = basic_block.values[instruction->out_slots[out_slot]];

        value->create_ref.instruction = nullptr;
        if(!value->use_refs.empty()) {
          ATOM_PANIC("IR instruction cannot be removed because it has active uses");
        }
      }

      // Remove use refs from any values read by this instruction
      for(size_t arg_slot = 0u; arg_slot < instruction->arg_slot_count; arg_slot++) {
        const Input& input = instruction->arg_slots[arg_slot];
        if(input.Is(Input::Type::Value)) {
          std::vector<Ref>& use_refs = basic_block.values[input.AsValue()]->use_refs;

          const auto match = std::ranges::find_if(use_refs, [&](const Ref& ref) {
            return ref.instruction == instruction && ref.slot == arg_slot;
          });
          if(match != use_refs.end()) {
            use_refs.erase(match);
          }
        }
      }

      UnlinkInstruction(basic_block, instruction);
    }

    static void UnlinkInstruction(BasicBlock& basic_block, Instruction* instruction) {
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

    static std::optional<u32> TryGetConst(BasicBlock& basic_block, Value::ID value_id) {
      Instruction* src_instruction = basic_block.values[value_id]->create_ref.instruction;
      if(src_instruction->type == Instruction::Type::LDCONST) {
        return src_instruction->GetArg(0u).AsConstU32();
      }
      return std::nullopt;
    }
};

} // namespace dual::arm::jit::ir

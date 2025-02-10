
#include <vector>

#include "host_flag_propagation_pass.hpp"

namespace dual::arm::jit::ir {

void HostFlagPropagationPass::Run(Function& function) {
  for(BasicBlock* basic_block : function.basic_blocks) {
    RunBasicBlock(*basic_block);
  }
}

void HostFlagPropagationPass::RunBasicBlock(BasicBlock& basic_block) {
  std::vector<FlagState> value_flag_states{};
  value_flag_states.resize(basic_block.values.size());

  Instruction* instruction = basic_block.head;

  while(instruction != nullptr) {
    switch(instruction->type) {
      case Instruction::Type::CVT_HFLAG_NZCV: {
        const Value::ID hflag_value = instruction->GetArg(0u).AsValue();
        const Value::ID nzcv_value = instruction->GetOut(0u);
        value_flag_states[nzcv_value] = {
          .n_flag = hflag_value,
          .z_flag = hflag_value,
          .c_flag = hflag_value,
          .v_flag = hflag_value
        };
        break;
      }
      case Instruction::Type::CVT_NZCV_HFLAG: {
        const Value::ID nzcv_value = instruction->GetArg(0u).AsValue();
        const Value::ID hflag_value = instruction->GetOut(0u);
        value_flag_states[hflag_value] = value_flag_states[nzcv_value];
        break;
      }
      case Instruction::Type::BR_IF: {
        // !! TODO !! only check the flags which are relevant for the condition code.
        const Value::ID hflag_value = instruction->GetArg(1u).AsValue();
        const FlagState& flag_state = value_flag_states[hflag_value];
        if(flag_state.n_flag != Value::invalid_id && flag_state.z_flag != Value::invalid_id && flag_state.c_flag != Value::invalid_id && flag_state.v_flag != Value::invalid_id) {
          RewriteValueUseRefs(basic_block, hflag_value, flag_state.n_flag);
        }
        break;
      }
      case Instruction::Type::BITCMB: {
        const Value::ID mask_value = instruction->GetArg(2u).AsValue();
        const std::optional<u32> mask_const_maybe = TryGetConst(basic_block, mask_value);

        if(mask_const_maybe.has_value()) {
          const u32 mask = mask_const_maybe.value();

          const FlagState& lhs_flag_state = value_flag_states[instruction->GetArg(0u).AsValue()];
          const FlagState& rhs_flag_state = value_flag_states[instruction->GetArg(1u).AsValue()];
          FlagState& result_flag_state = value_flag_states[instruction->GetOut(0u)];

          result_flag_state.n_flag = (mask & 0x80000000u) ? rhs_flag_state.n_flag : lhs_flag_state.n_flag;
          result_flag_state.z_flag = (mask & 0x40000000u) ? rhs_flag_state.z_flag : lhs_flag_state.z_flag;
          result_flag_state.c_flag = (mask & 0x20000000u) ? rhs_flag_state.c_flag : lhs_flag_state.c_flag;
          result_flag_state.v_flag = (mask & 0x10000000u) ? rhs_flag_state.v_flag : lhs_flag_state.v_flag;
        }
        break;
      }
      default: {
        break;
      }
    }

    instruction = instruction->next;
  }
}

} // namespace dual::arm::jit::ir

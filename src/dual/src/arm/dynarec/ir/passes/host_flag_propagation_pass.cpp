
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
      case Instruction::Type::ORR: {
        // !! TODO !! this is really hacky, handle flag merges properly!
        // Also if we do end up going with BIC, AND and OR, at least check that the ORR instruction is relevant.
        const Value::ID lhs_reg = instruction->GetArg(0u).AsValue();
        const Value::ID rhs_reg = instruction->GetArg(1u).AsValue();
        const Value::ID result_reg = instruction->GetOut(0u);
        const u32 nzcv_mask = 0xF0000000u; // yup, hardcoded mask!
        value_flag_states[result_reg].n_flag = (nzcv_mask & 0x80000000u) ? value_flag_states[rhs_reg].n_flag : value_flag_states[lhs_reg].n_flag;
        value_flag_states[result_reg].z_flag = (nzcv_mask & 0x40000000u) ? value_flag_states[rhs_reg].z_flag : value_flag_states[lhs_reg].z_flag;
        value_flag_states[result_reg].c_flag = (nzcv_mask & 0x20000000u) ? value_flag_states[rhs_reg].c_flag : value_flag_states[lhs_reg].c_flag;
        value_flag_states[result_reg].v_flag = (nzcv_mask & 0x10000000u) ? value_flag_states[rhs_reg].v_flag : value_flag_states[lhs_reg].v_flag;
        break;
      }
      case Instruction::Type::BR_IF: {
        // !! TODO !! only check the flags which are relevant for the condition code.
        const Value::ID hflag_value = instruction->GetArg(1u).AsValue();
        const FlagState& flag_state = value_flag_states[hflag_value];
        if(flag_state.n_flag != Value::invalid_id && flag_state.z_flag != Value::invalid_id && flag_state.c_flag != Value::invalid_id && flag_state.v_flag != Value::invalid_id) {
          fmt::print("can optimize BR_IF :^)\n");
          RewriteValueUseRefs(basic_block, hflag_value, flag_state.n_flag);
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

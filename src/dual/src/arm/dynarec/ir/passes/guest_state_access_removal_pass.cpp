
#include <array>

#include "arm/dynarec/ir/disassemble.hpp"
#include "arm/dynarec/ir/instruction.hpp"
#include "guest_state_access_removal_pass.hpp"

namespace dual::arm::jit::ir {

void GuestStateAccessRemovalPass::Run(Function& function) {
  for(BasicBlock* basic_block : function.basic_blocks) {
    RunBasicBlock(*basic_block);
  }
}

void GuestStateAccessRemovalPass::RunBasicBlock(BasicBlock& basic_block) {
  static constexpr auto id = [](GPR gpr, Mode cpu_mode) constexpr {
    const int max_shared_gpr = cpu_mode == Mode::FIQ ? 7 : 12;
    if((int)gpr <= max_shared_gpr || gpr == GPR::PC || cpu_mode == Mode::User) {
      return (int)gpr | (int)Mode::System << 4;
    }
    return (int)gpr | (int)cpu_mode << 4;
  };

  static constexpr auto cpsr_id = 0;

  // Track which IR value (if any) contains the current value of each guest GPR and PSR.
  std::array<Value::ID, 512u> current_gpr_value{};
  std::array<Value::ID,  32u> current_psr_value{};

  // Track which IR instruction (if any) most recently wrote to a guest GPR or PSR.
  std::array<Instruction*, 512u> most_recent_gpr_store{};
  std::array<Instruction*,  32u> most_recent_psr_store{};

  current_gpr_value.fill(Value::invalid_id);
  current_psr_value.fill(Value::invalid_id);

  Instruction* instruction = basic_block.head;

  while(instruction != nullptr) {
    switch(instruction->type) {
      case Instruction::Type::LDGPR: {
        const GPR gpr = instruction->GetArg(0u).AsGPR();
        const Mode cpu_mode = instruction->GetArg(1u).AsMode();
        const Value::ID result_value = instruction->GetOut(0u);

        const size_t gpr_id = id(gpr, cpu_mode);

        if(current_gpr_value[gpr_id] == Value::invalid_id) {
          current_gpr_value[gpr_id] = result_value;
        } else {
          RewriteValueUseRefs(basic_block, result_value, current_gpr_value[gpr_id]);
          RemoveInstruction(basic_block, instruction);
        }
        break;
      }
      case Instruction::Type::STGPR: {
        const GPR gpr = instruction->GetArg(0u).AsGPR();
        const Mode cpu_mode = instruction->GetArg(1u).AsMode();
        const Value::ID stored_value = instruction->GetArg(2u).AsValue();

        const size_t gpr_id = id(gpr, cpu_mode);

        current_gpr_value[gpr_id] = stored_value;

        if(most_recent_gpr_store[gpr_id]) {
          RemoveInstruction(basic_block, most_recent_gpr_store[gpr_id]);
        }
        most_recent_gpr_store[gpr_id] = instruction;
        break;
      }
      case Instruction::Type::LDCPSR: {
        const Value::ID result_value = instruction->GetOut(0u);

        if(current_psr_value[cpsr_id] == Value::invalid_id) {
          current_psr_value[cpsr_id] = result_value;
        } else {
          RewriteValueUseRefs(basic_block, result_value, current_psr_value[cpsr_id]);
          RemoveInstruction(basic_block, instruction);
        }
        break;
      }
      case Instruction::Type::STCPSR: {
        const Value::ID stored_value = instruction->GetArg(0u).AsValue();

        current_psr_value[cpsr_id] = stored_value;

        if(most_recent_psr_store[cpsr_id]) {
          RemoveInstruction(basic_block, most_recent_psr_store[cpsr_id]);
        }
        most_recent_psr_store[cpsr_id] = instruction;
        break;
      }
      case Instruction::Type::LDSPSR: {
        const Mode cpu_mode = instruction->GetArg(0u).AsMode();
        const Value::ID result_value = instruction->GetOut(0u);

        if(current_psr_value[(int)cpu_mode] == Value::invalid_id) {
          current_psr_value[(int)cpu_mode] = result_value;
        } else {
          RewriteValueUseRefs(basic_block, result_value, current_psr_value[(int)cpu_mode]);
          RemoveInstruction(basic_block, instruction);
        }
        break;
      }
      case Instruction::Type::STSPSR: {
        const Mode cpu_mode = instruction->GetArg(0u).AsMode();
        const Value::ID stored_value = instruction->GetArg(1u).AsValue();

        current_psr_value[(int)cpu_mode] = stored_value;

        if(most_recent_psr_store[(int)cpu_mode]) {
          RemoveInstruction(basic_block, most_recent_psr_store[(int)cpu_mode]);
        }
        most_recent_psr_store[(int)cpu_mode] = instruction;
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

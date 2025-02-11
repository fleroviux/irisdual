
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
  /**
   * Register ID layout:
   * Bits 0 to 4: register name (R0 to R15, SPSR, CPSR)
   * Bits 5 to 9: CPU mode (for CPSR this is zero)
   */

  static constexpr auto gpr_id = [](GPR gpr, Mode cpu_mode) constexpr {
    const int max_shared_gpr = cpu_mode == Mode::FIQ ? 7 : 12;
    if((int)gpr <= max_shared_gpr || gpr == GPR::PC || cpu_mode == Mode::User) {
      return (size_t)Mode::System << 5 | (size_t)gpr;
    }
    return (size_t)cpu_mode << 5 | (size_t)gpr;
  };

  static constexpr auto spsr_id = [](Mode cpu_mode) constexpr {
    return (size_t)cpu_mode << 5 | 16u;
  };

  static constexpr auto cpsr_id = 17u;

  // Track which IR value (if any) contains the current value of each guest GPR and PSR.
  std::array<Value::ID, 1024u> current_reg_value{};
  current_reg_value.fill(Value::invalid_id);

  // Track which IR instruction (if any) most recently wrote to a guest GPR or PSR.
  std::array<Instruction*, 1024u> most_recent_reg_store{};

  const auto HandleLdReg = [&](Instruction* instruction, size_t reg_id, Value::ID result_value) {
    if(current_reg_value[reg_id] == Value::invalid_id) {
      current_reg_value[reg_id] = result_value;
    } else {
      RewriteValueUseRefs(basic_block, result_value, current_reg_value[reg_id]);
      RemoveInstruction(basic_block, instruction); // TODO: just let dead code removal take care of this?
    }
  };

  const auto HandleStReg = [&](Instruction* instruction, size_t reg_id, Value::ID stored_value) {
    current_reg_value[reg_id] = stored_value;

    if(most_recent_reg_store[reg_id]) {
      RemoveInstruction(basic_block, most_recent_reg_store[reg_id]);
    }
    most_recent_reg_store[reg_id] = instruction;
  };

  Instruction* instruction = basic_block.head;

  while(instruction != nullptr) {
    switch(instruction->type) {
      case Instruction::Type::LDGPR: {
        const GPR gpr = instruction->GetArg(0u).AsGPR();
        const Mode cpu_mode = instruction->GetArg(1u).AsMode();
        const Value::ID result_value = instruction->GetOut(0u);
        HandleLdReg(instruction, gpr_id(gpr, cpu_mode), result_value);
        break;
      }
      case Instruction::Type::STGPR: {
        const GPR gpr = instruction->GetArg(0u).AsGPR();
        const Mode cpu_mode = instruction->GetArg(1u).AsMode();
        const Value::ID stored_value = instruction->GetArg(2u).AsValue();
        HandleStReg(instruction, gpr_id(gpr, cpu_mode), stored_value);
        break;
      }
      case Instruction::Type::LDCPSR: {
        const Value::ID result_value = instruction->GetOut(0u);
        HandleLdReg(instruction, cpsr_id, result_value);
        break;
      }
      case Instruction::Type::STCPSR: {
        const Value::ID stored_value = instruction->GetArg(0u).AsValue();
        HandleStReg(instruction, cpsr_id, stored_value);
        break;
      }
      case Instruction::Type::LDSPSR: {
        const Mode cpu_mode = instruction->GetArg(0u).AsMode();
        const Value::ID result_value = instruction->GetOut(0u);
        HandleLdReg(instruction, spsr_id(cpu_mode), result_value);
        break;
      }
      case Instruction::Type::STSPSR: {
        const Mode cpu_mode = instruction->GetArg(0u).AsMode();
        const Value::ID stored_value = instruction->GetArg(1u).AsValue();
        HandleStReg(instruction, spsr_id(cpu_mode), stored_value);
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

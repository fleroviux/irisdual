
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
    const int max_sys_gpr = cpu_mode == Mode::FIQ ? 7 : 12;
    if((int)gpr <= max_sys_gpr || gpr == GPR::PC || cpu_mode == Mode::User) {
      return (int)gpr | (int)Mode::System << 4;
    }
    return (int)gpr | (int)cpu_mode << 4;
  };

  std::array<Value::ID, 512u> gpr_value{};
  std::array<Value::ID, 32u> spsr_value{};
  Value::ID cpsr_value{Value::invalid_id};

  std::array<Instruction*, 512u> gpr_store_instruction{};
  std::array<Instruction*, 32u> spsr_store_instruction{};
  Instruction* cpsr_store_instruction{};

  gpr_value.fill(Value::invalid_id);
  spsr_value.fill(Value::invalid_id);

  Instruction* instruction = basic_block.head;

  while(instruction != nullptr) {
    switch(instruction->type) {
      case Instruction::Type::LDGPR: {
        const GPR gpr = instruction->GetArg(0u).AsGPR();
        const Mode cpu_mode = instruction->GetArg(1u).AsMode();
        const Value::ID value = instruction->GetOut(0u);
        if(gpr_value[id(gpr, cpu_mode)] == Value::invalid_id) {
          gpr_value[id(gpr, cpu_mode)] = value;
        } else {
          ReplaceValueUses(basic_block, value, gpr_value[id(gpr, cpu_mode)]);
          RemoveInstruction(basic_block, instruction);
        }
        break;
      }
      case Instruction::Type::STGPR: {
        const GPR gpr = instruction->GetArg(0u).AsGPR();
        const Mode cpu_mode = instruction->GetArg(1u).AsMode();
        const Value::ID value = instruction->GetArg(2u).AsValue();
        gpr_value[id(gpr, cpu_mode)] = value;
        if(gpr_store_instruction[id(gpr, cpu_mode)]) {
          RemoveInstruction(basic_block, gpr_store_instruction[id(gpr, cpu_mode)]);
        }
        gpr_store_instruction[id(gpr, cpu_mode)] = instruction;
        break;
      }
      case Instruction::Type::LDCPSR: {
        const Value::ID value = instruction->GetOut(0u);
        if(cpsr_value == Value::invalid_id) {
          cpsr_value = value;
        } else {
          ReplaceValueUses(basic_block, value, cpsr_value);
          RemoveInstruction(basic_block, instruction);
        }
        break;
      }
      case Instruction::Type::STCPSR: {
        const Value::ID value = instruction->GetArg(0u).AsValue();
        cpsr_value = value;
        if(cpsr_store_instruction) {
          RemoveInstruction(basic_block, cpsr_store_instruction);
        }
        cpsr_store_instruction = instruction;
        break;
      }
      case Instruction::Type::LDSPSR: {
        const Mode cpu_mode = instruction->GetArg(0u).AsMode();
        const Value::ID value = instruction->GetOut(0u);
        if(spsr_value[(int)cpu_mode] == Value::invalid_id) {
          spsr_value[(int)cpu_mode] = value;
        } else {
          ReplaceValueUses(basic_block, value, spsr_value[(int) cpu_mode]);
          RemoveInstruction(basic_block, instruction);
        }
        break;
      }
      case Instruction::Type::STSPSR: {
        const Mode cpu_mode = instruction->GetArg(0u).AsMode();
        const Value::ID value = instruction->GetArg(1u).AsValue();
        spsr_value[(int)cpu_mode] = value;
        if(spsr_store_instruction[(int)cpu_mode]) {
          RemoveInstruction(basic_block, spsr_store_instruction[(int) cpu_mode]);
        }
        spsr_store_instruction[(int)cpu_mode] = instruction;
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

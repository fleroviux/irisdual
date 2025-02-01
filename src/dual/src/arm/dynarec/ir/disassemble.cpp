
#include <atom/panic.hpp>
#include <fmt/format.h>

#include "disassemble.hpp"

namespace dual::arm::jit::ir {

static const char* get_instruction_mnemonic(Instruction::Type type) {
  switch(type) {
    case Instruction::Type::LDCONST: return "ldconst";
    case Instruction::Type::LDGPR:   return "ldgpr";
    case Instruction::Type::STGPR:   return "stgpr";
    case Instruction::Type::LDCPSR:  return "ldcpsr";
    case Instruction::Type::STCPSR:  return "stcpsr";
    case Instruction::Type::LDSPSR:  return "ldspsr";
    case Instruction::Type::STSPSR:  return "stspsr";
    case Instruction::Type::CVT_HFLAG_NZCV: return "cvt.hflag.nzcv";
    case Instruction::Type::BR:    return "br";
    case Instruction::Type::BR_IF: return "br_if";
    case Instruction::Type::EXIT:  return "exit";
    case Instruction::Type::BIC:   return "bic";
    case Instruction::Type::ADD:   return "add";
    case Instruction::Type::ORR:   return "orr";
    default: ATOM_PANIC("unhandled instruction type: {}", (int)type);
  }
}

static const char* get_cpu_mode_label(Mode cpu_mode) {
  switch(cpu_mode) {
    case Mode::User:       return "usr";
    case Mode::FIQ:        return "fiq";
    case Mode::IRQ:        return "irq";
    case Mode::Supervisor: return "svc";
    case Mode::Abort:      return "abt";
    case Mode::Undefined:  return "udf";
    case Mode::System:     return "sys";
    default: ATOM_PANIC("unhandled CPU mode: {}", (int)cpu_mode);
  }
}

static const char* get_condition_label(Condition condition) {
  switch(condition) {
    case Condition::EQ: return "eq";
    case Condition::NE: return "ne";
    case Condition::CS: return "cs";
    case Condition::CC: return "cc";
    case Condition::MI: return "mi";
    case Condition::PL: return "pl";
    case Condition::VS: return "vs";
    case Condition::VC: return "vc";
    case Condition::HI: return "hi";
    case Condition::LS: return "ls";
    case Condition::GE: return "ge";
    case Condition::LT: return "lt";
    case Condition::GT: return "gt";
    case Condition::LE: return "le";
    default: ATOM_PANIC("unhandled condition: {}", (int)condition);
  }
}

std::string disassemble(const BasicBlock& basic_block) {
  std::string disassembled_code{};
  disassembled_code.reserve(4096); // 4 KiB should be enough to fit near all (if not all) cases

  const Instruction* instruction = basic_block.head;

  while(instruction != nullptr) {
    const size_t out_slot_count = instruction->out_slot_count;
    const size_t arg_slot_count = instruction->arg_slot_count;

    if(out_slot_count != 0u) {
      for(size_t slot = 0; slot < out_slot_count; slot++) {
        disassembled_code += fmt::format("v{}", instruction->out_slots[slot]);
        if(slot != out_slot_count - 1) {
          disassembled_code += ", ";
        }
      }

      disassembled_code += "\t := ";
    } else {
      disassembled_code += "\t    "; // indent to same level as other instructions
    }

    disassembled_code += get_instruction_mnemonic(instruction->type);
    if(instruction->flags) {
      disassembled_code += ".";
      if(instruction->flags & Instruction::Flag::OutputHostFlags) disassembled_code += "s";
    }
    disassembled_code += " ";

    for(size_t slot = 0; slot < arg_slot_count; slot++) {
      const Input& arg = instruction->arg_slots[slot];

      switch(arg.GetType()) {
        case Input::Type::Null: {
          disassembled_code += "(null)";
          break;
        }
        case Input::Type::Value: {
          disassembled_code += fmt::format("v{}", arg.AsValue());
          break;
        }
        case Input::Type::GPR: {
          disassembled_code += fmt::format("r{}", (int)arg.AsGPR());
          break;
        }
        case Input::Type::Mode: {
          disassembled_code += "%";
          disassembled_code += get_cpu_mode_label(arg.AsMode());
          break;
        }
        case Input::Type::ConstU32: {
          disassembled_code += fmt::format("0x{:08X}_u32", arg.AsConstU32());
          break;
        }
        case Input::Type::Condition: {
          disassembled_code += get_condition_label(arg.AsCondition());
          break;
        }
        default: {
          ATOM_PANIC("unhandled input type: {}", (int)arg.GetType());
        }
      }

      if(slot != arg_slot_count - 1) {
        disassembled_code += ", ";
      }
    }

    disassembled_code += "\n";

    instruction = instruction->next;
  }

  return disassembled_code;
}

} // namespace dual::arm::jit::ir
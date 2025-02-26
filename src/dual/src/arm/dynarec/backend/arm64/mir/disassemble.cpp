
#include <atom/panic.hpp>

#include "disassemble.hpp"
#include "instruction.hpp"

namespace dual::arm::jit::a64mir {

static const char* get_instruction_mnemonic(Instruction::Type type);
static std::string disassemble_operand(const Operand& operand);

std::string disassemble(const BasicBlock& basic_block, const char* indent) {
  return "TODO";
}

std::string disassemble(const Instruction& instruction) {
  const size_t out_slot_count = instruction.out_slot_count;
  const size_t arg_slot_count = instruction.arg_slot_count;

  std::string disassembled_code{};
  disassembled_code.reserve(100);

  if(out_slot_count > 0) {
    for(size_t out_slot = 0u; out_slot < out_slot_count; out_slot++) {
      disassembled_code += disassemble_operand(instruction.out_slots[out_slot]);
      if(out_slot != out_slot_count - 1u) {
        disassembled_code += ", ";
      }
    }

    disassembled_code += "\t := ";
  } else {
    disassembled_code += "\t    ";
  }

  disassembled_code += get_instruction_mnemonic(instruction.type);
  disassembled_code += " ";

  for(size_t arg_slot = 0u; arg_slot < arg_slot_count; arg_slot++) {
    disassembled_code += disassemble_operand(instruction.arg_slots[arg_slot]);
    if(arg_slot != arg_slot_count - 1u) {
      disassembled_code += ", ";
    }
  }

  return disassembled_code;
}

static const char* get_instruction_mnemonic(Instruction::Type type) {
  switch(type) {
    case Instruction::Type::LDR: return "ldr";
    case Instruction::Type::STR: return "str";
    default: ATOM_PANIC("unhandled instruction type: {}", (int)type);
  }
}

static std::string disassemble_operand(const Operand& operand) {
  switch(operand.GetType()) {
    case Operand::Type::Null: {
      return "(null)";
    }
    case Operand::Type::HostReg: {
      // TODO: move this into a function or something
      HostReg host_reg = operand.AsHostReg();
      switch(host_reg.GetType()) {
        case HostReg::Type::WReg: return fmt::format("W{}", host_reg.Index());
        case HostReg::Type::XReg: return fmt::format("X{}", host_reg.Index());
        default: ATOM_PANIC("unhandled host reg type: {}", (int)host_reg.GetType());
      }
    }
    case Operand::Type::Address: {
      // TODO: move this into a function or something
      const a64mir::Address& address = operand.AsAddress();
      std::string offset_str;
      if(address.offset.IsImm()) {
        offset_str = fmt::format("#{}", address.offset.GetImm());
      } else {
        const a64mir::AddressOffset::RegOffset& reg_offset = address.offset.GetReg();
        offset_str = fmt::format("X{}", reg_offset.reg.index());
        if(reg_offset.ext != oaknut::IndexExt::LSL || reg_offset.ext_imm != 0) {
          switch(reg_offset.ext) {
            case oaknut::IndexExt::LSL:  offset_str += ", lsl, ";  break;
            case oaknut::IndexExt::UXTW: offset_str += ", uxtw, "; break;
            case oaknut::IndexExt::SXTW: offset_str += ", sxtw, "; break;
            case oaknut::IndexExt::SXTX: offset_str += ", sxtx, "; break;
          }
          offset_str += fmt::format("#{}", reg_offset.ext_imm);
        }
      }

      switch(address.mode) {
        case Address::BasePlusOffset: return fmt::format("[X{}, {}]",  address.reg_base.index(), offset_str);
        case Address::PreIndexed:     return fmt::format("[X{}, {}]!", address.reg_base.index(), offset_str);
        case Address::PostIndexed:    return fmt::format("[X{}], {}",  address.reg_base.index(), offset_str); // TODO: this is ambiguous!
      }
      break;
    }
    default: {
      ATOM_PANIC("unhandled MIR operand type: {}", (int)operand.GetType());
    }
  }
}

} // namespace dual::arm::jit::a64mir

#include <atom/panic.hpp>

#include "disassemble.hpp"
#include "instruction.hpp"

namespace dual::arm::jit::a64mir {

static const char* get_instruction_mnemonic(Instruction::Type type);

std::string disassemble(const BasicBlock& basic_block, const char* indent) {
  return "";
}

std::string disassemble(const Instruction& instruction) {
  const size_t out_slot_count = instruction.out_slot_count;
  const size_t arg_slot_count = instruction.arg_slot_count;

  std::string disassembled_code{};
  disassembled_code.reserve(100);

  // TODO: outputs

  disassembled_code += get_instruction_mnemonic(instruction.type);
  disassembled_code += " ";

  for(size_t arg_slot = 0u; arg_slot < instruction.arg_slot_count; arg_slot++) {
    const a64mir::Operand& arg = instruction.arg_slots[arg_slot];

    switch(arg.GetType()) {
      case a64mir::Operand::Type::Address: {
        const a64mir::Address& address = arg.AsAddress();
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
          case Address::BasePlusOffset: disassembled_code += fmt::format("[X{}, {}]",  address.reg_base.index(), offset_str); break;
          case Address::PreIndexed:     disassembled_code += fmt::format("[X{}, {}]!", address.reg_base.index(), offset_str); break;
          case Address::PostIndexed:    disassembled_code += fmt::format("[X{}], {}",  address.reg_base.index(), offset_str); break;
        }
        break;
      }
      default: {
        ATOM_PANIC("unhandled MIR operand type: {}", (int)arg.GetType());
      }
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

} // namespace dual::arm::jit::a64mir
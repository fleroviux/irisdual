
#include <atom/arena.hpp>
#include <atom/panic.hpp>

#include "arm/dynarec/ir/basic_block.hpp"
#include "arm/dynarec/ir/instruction.hpp"
#include "mir/basic_block.hpp"
#include "mir/emitter.hpp"
#include "mir/instruction.hpp"
#include "arm64_backend.hpp"

namespace dual::arm::jit {

void ARM64Backend::LowerToMIR(const ir::BasicBlock& basic_block) {
  //return;

  atom::Arena memory_arena{16384u};

  // Allocate MIR basic block
  const auto mir_basic_block = (a64mir::BasicBlock*)memory_arena.Allocate(sizeof(a64mir::BasicBlock));
  if(mir_basic_block == nullptr) [[unlikely]] {
    ATOM_PANIC("ran out of memory arena space");
  }
  new(mir_basic_block) a64mir::BasicBlock{basic_block.id};

  // Create MIR emitter
  a64mir::Emitter mir_emitter{*mir_basic_block, memory_arena};

  using namespace oaknut::util;
  mir_emitter.LDR(X0, a64mir::AddressOffset{0u});
  mir_emitter.LDR(X0, a64mir::AddressOffset{0u}, a64mir::Address::Mode::PostIndexed);
  mir_emitter.LDR(X0, a64mir::AddressOffset{X1});
  mir_emitter.LDR(X0, a64mir::AddressOffset{X1, oaknut::IndexExt::UXTW, 3});

  // Disassembly test
  a64mir::Instruction* instruction = mir_basic_block->head;
  while(instruction != nullptr) {
    switch(instruction->type) {
      case a64mir::Instruction::Type::LDR: fmt::print("ldr "); break;
      default: {
        ATOM_PANIC("unhandled MIR instruction type: {}", (int)instruction->type);
      }
    }

    for(size_t arg_slot = 0u; arg_slot < instruction->arg_slot_count; arg_slot++) {
      const a64mir::Operand& arg = instruction->arg_slots[arg_slot];

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
                case oaknut::IndexExt::LSL: offset_str += ", lsl, "; break;
                case oaknut::IndexExt::UXTW: offset_str += ", uxtw, "; break;
                case oaknut::IndexExt::SXTW: offset_str += ", sxtw, "; break;
                case oaknut::IndexExt::SXTX: offset_str += ", sxtx, "; break;
              }
              offset_str += fmt::format("#{}", reg_offset.ext_imm);
            }
          }

          switch(address.mode) {
            case a64mir::Address::BasePlusOffset: fmt::print("[X{}, {}]", address.reg_base.index(), offset_str); break;
            case a64mir::Address::PreIndexed: fmt::print("[X{}, {}]!", address.reg_base.index(), offset_str); break;
            case a64mir::Address::PostIndexed: fmt::print("[X{}], {}", address.reg_base.index(), offset_str); break;
          }
          break;
        }
        default: {
          ATOM_PANIC("unhandled MIR operand type: {}", (int)arg.GetType());
        }
      }
    }

    // TODO: outputs

    fmt::print("\n");

    instruction = instruction->next;
  }

//  // Lower IR instructions to MIR instructions
//  ir::Instruction* instruction = basic_block.head;
//  while(instruction != nullptr) {
//    switch(instruction->type) {
//      default: {
//        ATOM_PANIC("unhandled IR instruction type: {}", (int)instruction->type);
//      }
//    }
//
//    instruction = instruction->next;
//  }

  fmt::print("lowering done!\n");
}


} // namespace dual::arm::jit

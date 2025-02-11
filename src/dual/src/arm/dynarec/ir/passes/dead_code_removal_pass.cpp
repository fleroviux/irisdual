
#include <atom/panic.hpp>

#include "dead_code_removal_pass.hpp"

#include "arm/dynarec/ir/disassemble.hpp"

namespace dual::arm::jit::ir {

static bool IsSideEffectFree(Instruction* instruction);

void DeadCodeRemovalPass::Run(Function& function) {
  for(BasicBlock* basic_block : function.basic_blocks) {
    RunBasicBlock(*basic_block);
  }
}

void DeadCodeRemovalPass::RunBasicBlock(BasicBlock& basic_block) {
  Instruction* instruction = basic_block.head;

  // TODO(fleroviux): repeatedly iterate over the IR until no dead code was found?
  while(instruction != nullptr) {
    // Only consider instructions without side-effects for removal
    if(IsSideEffectFree(instruction)) {
      // Figure out how many references to the values created by the instruction exist.
      size_t uses = 0;
      for(size_t out_slot = 0u; out_slot < instruction->out_slot_count; out_slot++) {
        uses += basic_block.values[instruction->GetOut(out_slot)]->use_refs.size();
      }

      // If none of the created values have any uses (and the instruction is side-effect free), remove it.
      if(uses == 0u) {
        RemoveInstruction(basic_block, instruction);
      }
    }

    instruction = instruction->next;
  }
}

static bool IsSideEffectFree(Instruction* instruction) {
  using Type = Instruction::Type;

  switch(instruction->type) {
    case Type::LDGPR:
    case Type::LDCPSR:
    case Type::LDSPSR:
    case Type::LDCONST:
    case Type::CVT_HFLAG_NZCV:
    case Type::CVT_NZCV_HFLAG:
    case Type::LSL:
    case Type::LSR:
    case Type::ASR:
    case Type::ROR:
    case Type::RRX:
    case Type::AND:
    case Type::BIC:
    case Type::EOR:
    case Type::SUB:
    case Type::RSB:
    case Type::ADD:
    case Type::ADC:
    case Type::SBC:
    case Type::RSC:
    case Type::ORR:
    case Type::MOV:
    case Type::MVN:
    case Type::BITCMB: {
      return true;
    }
    case Type::STGPR:
    case Type::STCPSR:
    case Type::STSPSR:
    case Type::BR:
    case Type::BR_IF:
    case Type::EXIT: {
      return false;
    }
    default: {
      ATOM_PANIC("unhandled instruction type: {}", (int)instruction->type);
    }
  }
}

} // namespace dual::arm::jit::ir
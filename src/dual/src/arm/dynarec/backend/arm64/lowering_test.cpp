
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
  atom::Arena memory_arena{16384u};

  // Allocate MIR basic block
  const auto mir_basic_block = (a64mir::BasicBlock*)memory_arena.Allocate(sizeof(a64mir::BasicBlock));
  if(mir_basic_block == nullptr) [[unlikely]] {
    ATOM_PANIC("ran out of memory arena space");
  }
  new(mir_basic_block) a64mir::BasicBlock{basic_block.id};

  // Create MIR emitter
  a64mir::Emitter mir_emitter{*mir_basic_block, memory_arena};

  // Lower IR instructions to MIR instructions
  ir::Instruction* instruction = basic_block.head;
  while(instruction != nullptr) {
    switch(instruction->type) {
      default: {
        ATOM_PANIC("unhandled IR instruction type: {}", (int)instruction->type);
      }
    }

    instruction = instruction->next;
  }

  fmt::print("lowering done!\n");
}


} // namespace dual::arm::jit

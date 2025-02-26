
#include "arm/dynarec/ir/value.hpp"
#include "mir/basic_block.hpp"
#include "mir/disassemble.hpp"
#include "mir/emitter.hpp"
#include "arm64_lowering_pass.hpp"

namespace dual::arm::jit {

void ARM64LoweringPass::Run(const ir::BasicBlock& basic_block, atom::Arena& memory_arena) {
  // Allocate MIR basic block
  const auto mir_basic_block = (a64mir::BasicBlock*)memory_arena.Allocate(sizeof(a64mir::BasicBlock));
  if(mir_basic_block == nullptr) [[unlikely]] {
    ATOM_PANIC("ran out of memory arena space");
  }
  new(mir_basic_block) a64mir::BasicBlock{basic_block.id};

  // Create MIR emitter
  a64mir::Emitter mir_emitter{*mir_basic_block, memory_arena};

//  using namespace oaknut::util;
//  mir_emitter.LDR(X0, a64mir::AddressOffset{0u});
//  mir_emitter.LDR(X0, a64mir::AddressOffset{0u}, a64mir::Address::Mode::PostIndexed);
//  mir_emitter.LDR(X0, a64mir::AddressOffset{X1});
//  const a64mir::VReg& vreg = mir_emitter.LDR(X0, a64mir::AddressOffset{X1, oaknut::IndexExt::UXTW, 3});
//  mir_emitter.Test(vreg);

  // Disassembly test
  a64mir::Instruction* instruction = mir_basic_block->head;
  while(instruction != nullptr) {
    fmt::print("{}\n", a64mir::disassemble(*instruction));
    instruction = instruction->next;
  }
}

} // namespace dual::arm::jit
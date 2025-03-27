
#include <atom/bit.hpp>
#include <unordered_set>

#include "analyzer.hpp"

namespace bit = atom::bit;

namespace dual::arm::jit {

Analyzer::Analyzer(Memory& memory) : m_memory{memory} {
}

std::vector<Analyzer::BlockOfCode> Analyzer::AnalyzeT16(u32 function_entrypoint) {
  /**
   * Instructions that change control flow:
   * ~ Branch/exchange instruction set (indirect branch with exchange)
   * ~ Special Data Processing
   *   - ADD and MOV with DST=r15 (indirect branch with exchange)
   * ~ PushPopRegList
   *   - POP with r-bit set (indirect branch with potential exchange)
   * ~ Software breakpoint
   * ~ Undefined (potentially) (exception)
   * ~ Software Interrupt (exception)
   * ~ Conditional branch (direct branch, conditional)
   * ~ Unconditional branch (direct branch)
   * ~ BL/BLX suffix (indirect-ish branch! can be part of a direct branch, but can be abused to be indirect.)
   */

  // TODO(fleroviux): write instructions into a buffer so that we don't have to fetch more than once?
  // TODO(fleroviux): limit function size to avoid explosions if we're wrong about code vs data?

  std::vector<Analyzer::BlockOfCode> blocks_of_code{};

  const auto PushBlockOfCode = [&](u32 address_lo, u32 address_hi, Analyzer::Link link = {}) {
    const auto it = std::find_if(blocks_of_code.begin(), blocks_of_code.end(), [&](const Analyzer::BlockOfCode& block_of_code) {
      return address_lo <= block_of_code.address_hi;
    });
    if(it != blocks_of_code.end()) {
      if(address_lo == it->address_lo) {
        return; // I don't think this can actually happen
      }
      if(address_lo > it->address_lo) {
        const u32 saved_address_lo = it->address_lo;
        it->address_lo = address_lo;
        it->link = link;
        blocks_of_code.insert(it, {saved_address_lo, (u32)(address_lo - sizeof(u16))});
      } else {
        fmt::print("insert {:08X} before {:08X}\n", address_lo, it->address_lo);
        blocks_of_code.insert(it, {address_lo, address_hi});
      }
    } else {
      blocks_of_code.push_back({address_lo, address_hi, link});
    }
  };

  std::vector<u32> decompile_queue{function_entrypoint};
  std::unordered_set<u32> entrypoints_seen{};

  while(!decompile_queue.empty()) {
    // Pop one entry from the decompile queue
    u32 entrypoint = decompile_queue.front();;
    decompile_queue.erase(decompile_queue.begin());

    if(entrypoints_seen.contains(entrypoint)) {
      continue;
    }
    entrypoints_seen.insert(entrypoint);

    // Iterate instructions at address to identify further blocks of code
    u32 current_pc = entrypoint;
    for(int i = 0; i < 40; i++) { // loop limit is arbitrary for now
      const u16 instruction = m_memory.ReadHalf(current_pc, Memory::Bus::Code);
      fmt::print("inst 0x{:08X}: 0x{:04X}\n", current_pc, instruction);

      // TODO(fleroviux): can we optimize this ugly if-chain neatly? Use a second LUT?
      if(bit::match_pattern<"01000111????????">(instruction)) {
        // Branch/exchange instruction set
        // NOTES:
        // - can be used to indirectly call another function or subroutine (i.e. switch case)
        // - can also be used to return from a function, i.e.:
        //   - BX LR
        //   - POP {R1}; BX R1

        // 1. Finish current basic block
        // 2. Add heuristic to detect calls later, for now exit function here
        fmt::print("control flow change: Branch/exchange instruction set\n");
        PushBlockOfCode(entrypoint, current_pc);
        break;
      } else if(bit::match_pattern<"010001?01????111">(instruction)) {
        // Special Data Processing
        // ADD R15, ...
        // MOV R15, ...
        // 1. Finish current basic block
        // 2. Exit function here
        fmt::print("control flow change: Special Data Processing\n");
        PushBlockOfCode(entrypoint, current_pc);
        break;
      } else if(bit::match_pattern<"10111101????????">(instruction)) {
        // Push/pop register list
        // POP {..., R15}
        // 1. Finish current basic block
        // 2. Exit function here.
        fmt::print("control flow change: Push/pop register list\n");
        PushBlockOfCode(entrypoint, current_pc);
        break;
      } else if(bit::match_pattern<"10111110????????">(instruction)) {
        // Software breakpoint
        // 1. Finish current basic block
        // 2. Exit function here
        ATOM_PANIC("unhandled Thumb software breakpoint");
      } else if(bit::match_pattern<"11011110????????">(instruction) || bit::match_pattern<"11101??????????1">(instruction)) {
        // Undefined
        // 1. Finish current basic block
        // 2. Exit function here
        ATOM_PANIC("unhandled undefined instruction");
      } else if(bit::match_pattern<"11011111????????">(instruction)) {
        // Software interrupt
        // 1. Finish current basic block
        // 2. Submit new basic block at next instruction
        fmt::print("control flow change: Software interrupt\n");
        PushBlockOfCode(entrypoint, current_pc);
        decompile_queue.push_back(current_pc + sizeof(u16));
        break;
      } else if(bit::match_pattern<"1101????????????">(instruction)) {
        // Conditional branch
        const u32 imm_offset = (u32)(i32)(i8)bit::get_field(instruction, 0u, 8u) << 1;
        const ir::Condition condition = bit::get_field<u16, ir::Condition>(instruction, 8u, 4u);
        const u32 next_pc_true  = current_pc + imm_offset + sizeof(u16) * 2u;
        const u32 next_pc_false = current_pc + sizeof(u16);

        // 1. Finish current basic block
        // 2. Submit new basic block at both branch targets
        fmt::print("control flow change: Conditional branch (targets: 0x{:08X}, 0x{:08X})\n", next_pc_true, next_pc_false);
        PushBlockOfCode(entrypoint, current_pc, Analyzer::Link{condition, next_pc_true, next_pc_false});
        decompile_queue.push_back(next_pc_true);
        decompile_queue.push_back(next_pc_false);
        break;
      } else if(bit::match_pattern<"11100???????????">(instruction)) {
        // Unconditional branch
        u32 imm_offset = bit::get_field(instruction, 0u, 11u);
        if(imm_offset & 0x400u) {
          imm_offset |= 0xFFFFF800u;
        }
        const u32 next_pc = current_pc + (imm_offset << 1) + sizeof(u16) * 2u;

        // 1. Finish current basic block
        // 2. Submit new basic block at branch target
        fmt::print("control flow change: Unconditional branch (target: 0x{:08X})\n", next_pc);
        PushBlockOfCode(entrypoint, current_pc, Analyzer::Link{next_pc});
        decompile_queue.push_back(next_pc);
        break;
      } else if(bit::match_pattern<"111?1???????????">(instruction)) {
        // BL(X) suffix
        // 1. Finish current basic block
        // 2. Submit new basic block at next instruction
        fmt::print("control flow change: BL(X) suffix\n");
        PushBlockOfCode(entrypoint, current_pc);
        decompile_queue.push_back(current_pc + sizeof(u16));
        break;
      }

      current_pc += sizeof(u16);
    }
  }

  return blocks_of_code;
}

} // namespace dual::arm::jit

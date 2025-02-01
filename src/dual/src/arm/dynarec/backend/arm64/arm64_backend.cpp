
#include <atom/panic.hpp>
#include <algorithm>
#include <fmt/format.h>
#include <oaknut/code_block.hpp>
#include <oaknut/oaknut.hpp>
#include <capstone/capstone.h>
#include <vector>

#include "arm/dynarec/ir/disassemble.hpp"
#include "arm64_backend.hpp"
#include "arm64_value_location.hpp"

namespace dual::arm::jit {

static void DisasA64(const void* code_begin, const void* code_end);

static ir::Condition NegateCondition(ir::Condition condition);

ARM64Backend::ARM64Backend(State& cpu_state)
    : m_cpu_state{cpu_state} {
}

void ARM64Backend::Execute(const ir::Function& function) {
  using namespace oaknut::util;

  static constexpr auto XReg_State = X0;

  oaknut::CodeBlock code_block{4096u};
  oaknut::CodeGenerator code{code_block.ptr()};

  oaknut::Label label_exit{};
  std::vector<oaknut::Label> basic_block_labels{};
  basic_block_labels.resize(function.basic_blocks.size());

  const void* code_begin = code.xptr<void*>();
  code_block.unprotect();
  code.MOVP2R(XReg_State, &m_cpu_state);

  fmt::print("===========================================\n");
  fmt::print("IR function:\n{}\n", ir::disassemble(function));

  for(size_t bb_index = 0u; bb_index < function.basic_blocks.size(); bb_index++) {
    const ir::BasicBlock& basic_block = function.basic_blocks[bb_index];

    // here comes the poor girl's register allocator!
    std::vector<ARM64ValueLocation> location_map{};
    location_map.resize(basic_block.values.size());
    if(location_map.size() > 15) {
      ATOM_PANIC("out of registers");
    }
    for(int i = 0; i < location_map.size(); i++) {
      location_map[i] = ARM64ValueLocation{oaknut::WReg{i + 1}};
    }
    const auto GetLocation = [&](ir::Value::ID value_id) {
      return location_map[value_id];
    };

    ir::Instruction* instruction = basic_block.head;

    bool got_terminal_instruction = false;

    const size_t next_bb_index = bb_index + 1u;

    code.l(basic_block_labels[bb_index]);

    while(instruction != nullptr) {
      switch(instruction->type) {
        case ir::Instruction::Type::LDCONST: {
          const oaknut::WReg result_reg = GetLocation(instruction->GetOut(0u)).AsWReg();
          code.MOV(result_reg, instruction->GetArg(0u).AsConstU32());
          break;
        }
        case ir::Instruction::Type::LDGPR: {
          const ir::GPR gpr = instruction->GetArg(0u).AsGPR();
          const ir::Mode cpu_mode = instruction->GetArg(1u).AsMode();
          const oaknut::WReg result_reg = GetLocation(instruction->GetOut(0u)).AsWReg();
          code.LDR(result_reg, XReg_State, m_cpu_state.GetOffsetToGPR(gpr, cpu_mode));
          break;
        }
        case ir::Instruction::Type::STGPR: {
          const ir::GPR gpr = instruction->GetArg(0u).AsGPR();
          const ir::Mode cpu_mode = instruction->GetArg(1u).AsMode();
          const oaknut::WReg value_reg = GetLocation(instruction->GetArg(2u).AsValue()).AsWReg();
          code.STR(value_reg, XReg_State, m_cpu_state.GetOffsetToGPR(gpr, cpu_mode));
          break;
        }
        case ir::Instruction::Type::LDCPSR: {
          const oaknut::WReg result_reg = GetLocation(instruction->GetOut(0u)).AsWReg();
          code.LDR(result_reg, XReg_State, m_cpu_state.GetOffsetToCPSR());
          break;
        }
        case ir::Instruction::Type::STCPSR: {
          const oaknut::WReg value_reg = GetLocation(instruction->GetArg(0u).AsValue()).AsWReg();
          code.STR(value_reg, XReg_State, m_cpu_state.GetOffsetToCPSR());
          break;
        }
        case ir::Instruction::Type::LDSPSR: {
          const ir::Mode cpu_mode = instruction->GetArg(0u).AsMode();
          const oaknut::WReg result_reg = GetLocation(instruction->GetOut(0u)).AsWReg();
          code.LDR(result_reg, XReg_State, m_cpu_state.GetOffsetToSPSR(cpu_mode));
          break;
        }
        case ir::Instruction::Type::STSPSR: {
          const ir::Mode cpu_mode = instruction->GetArg(0u).AsMode();
          const oaknut::WReg value_reg = GetLocation(instruction->GetArg(1u).AsValue()).AsWReg();
          code.STR(value_reg, XReg_State, m_cpu_state.GetOffsetToSPSR(cpu_mode));
          break;
        }
        case ir::Instruction::Type::CVT_HFLAG_NZCV: {
          const oaknut::WReg hflags_reg = GetLocation(instruction->GetArg(0u).AsValue()).AsWReg();
          const oaknut::WReg nzcv_reg = GetLocation(instruction->GetOut(0u)).AsWReg();
          code.MOV(nzcv_reg, hflags_reg); // TODO(fleroviux): implement move elimination
          break;
        }
        case ir::Instruction::Type::BR: {
          const u32 target_bb = instruction->GetArg(0u).AsConstU32();
          if(target_bb != next_bb_index) {
            code.B(basic_block_labels[target_bb]);
          }

          got_terminal_instruction = true;
          break;
        }
        case ir::Instruction::Type::BR_IF: {
          const oaknut::WReg hflags_reg = GetLocation(instruction->GetArg(1u).AsValue()).AsWReg();

          ir::Condition condition = instruction->GetArg(0u).AsCondition();
          u32 bb_true  = instruction->GetArg(2u).AsConstU32();
          u32 bb_false = instruction->GetArg(3u).AsConstU32();

          // Negate the branch condition and branch targets if the 'true' branch target is the next basic block in order.
          // This saves us the unconditional branch to the 'false' branch target.
          if(bb_true == next_bb_index) {
            std::swap(bb_true, bb_false);
            condition = NegateCondition(condition);
          }

          // TODO(fleroviux): optimize this out when possible
          code.MSR(oaknut::SystemReg::NZCV, hflags_reg.toX());

          // Note that oaknut::Cond is equivalent to our condition enum.
          code.B((oaknut::Cond)condition, basic_block_labels[bb_true]);
          if(bb_false != next_bb_index) {
            code.B(basic_block_labels[bb_false]);
          }

          got_terminal_instruction = true;
          break;
        }
        case ir::Instruction::Type::EXIT: {
          // TODO(fleroviux): is there a better way to get rid of the branch when we don't need it?
          if(bb_index != function.basic_blocks.size() - 1u) {
            code.B(label_exit);
          }

          got_terminal_instruction = true;
          break;
        }
        case ir::Instruction::Type::BIC: {
          const oaknut::WReg lhs_reg = GetLocation(instruction->GetArg(0u).AsValue()).AsWReg();
          const oaknut::WReg rhs_reg = GetLocation(instruction->GetArg(1u).AsValue()).AsWReg();
          const oaknut::WReg result_reg = GetLocation(instruction->GetOut(0u)).AsWReg();

          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            // TODO(fleroviux): implement support for flag only output
            const oaknut::WReg hflags_reg = GetLocation(instruction->GetOut(1u)).AsWReg();
            code.BICS(result_reg, lhs_reg, rhs_reg);
            code.MRS(hflags_reg.toX(), oaknut::SystemReg::NZCV);
          } else {
            code.BIC(result_reg, lhs_reg, rhs_reg);
          }
          break;
        }
        case ir::Instruction::Type::ADD: {
          // TODO(fleroviux): this is very similar to BIC(S), can we deduplicate code reliably?
          const oaknut::WReg lhs_reg = GetLocation(instruction->GetArg(0u).AsValue()).AsWReg();
          const oaknut::WReg rhs_reg = GetLocation(instruction->GetArg(1u).AsValue()).AsWReg();
          const oaknut::WReg result_reg = GetLocation(instruction->GetOut(0u)).AsWReg();

          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            // TODO(fleroviux): implement support for flag only output
            const oaknut::WReg hflags_reg = GetLocation(instruction->GetOut(1u)).AsWReg();
            code.ADDS(result_reg, lhs_reg, rhs_reg);
            code.MRS(hflags_reg.toX(), oaknut::SystemReg::NZCV);
          } else {
            code.ADD(result_reg, lhs_reg, rhs_reg);
          }
          break;
        }
        case ir::Instruction::Type::ORR: {
          // TODO(fleroviux): this is very similar to BIC(S), can we deduplicate code reliably?
          const oaknut::WReg lhs_reg = GetLocation(instruction->GetArg(0u).AsValue()).AsWReg();
          const oaknut::WReg rhs_reg = GetLocation(instruction->GetArg(1u).AsValue()).AsWReg();
          const oaknut::WReg result_reg = GetLocation(instruction->GetOut(0u)).AsWReg();

          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            // TODO(fleroviux): implement support for flag only output
            const oaknut::WReg hflags_reg = GetLocation(instruction->GetOut(1u)).AsWReg();
            code.ORR(result_reg, lhs_reg, rhs_reg);
            code.TST(result_reg, result_reg);
            code.MRS(hflags_reg.toX(), oaknut::SystemReg::NZCV);
          } else {
            code.ORR(result_reg, lhs_reg, rhs_reg);
          }
          break;
        }
        default: {
          ATOM_PANIC("unhandled IR instruction type: {}", (int)instruction->type);
        }
      }

      instruction = instruction->next;
    }

    if(!got_terminal_instruction) {
      ATOM_PANIC("Basic block did not end in a terminal instruction!");
    }
  }

  code.l(label_exit);
  code.RET();
  code_block.protect();
  code_block.invalidate_all();

  fmt::print("Compiled Function (ARM64):\n");
  DisasA64(code_begin, code.xptr<void*>());
  fmt::print("===========================================\n");

  ((void (*)())code_begin)();
}

static void DisasA64(const void* code_begin, const void* code_end) {
  csh handle;
  if(cs_open(CS_ARCH_AARCH64, CS_MODE_ARM, &handle) != CS_ERR_OK) {
    ATOM_PANIC("failed to initialize capstone");
  }

  cs_insn* instructions;
  const size_t count = cs_disasm(handle, (const u8*)code_begin, (const u8*)code_end - (const u8*)code_begin, (u64)code_begin, 0, &instructions);
  for(size_t i = 0; i < count; i++) {
    fmt::print("0x{:x}: {} \t{}\n", instructions[i].address, instructions[i].mnemonic, instructions[i].op_str);
  }

  cs_free(instructions, count);
  cs_close(&handle);
}

static ir::Condition NegateCondition(ir::Condition condition) {
  // Each condition in the ir::Condition enum is followed by its negated condition, so we can negate bit #0 to negate the condition.
  return static_cast<ir::Condition>(static_cast<int>(condition) ^ 1);
}

} // namespace dual::arm::jit

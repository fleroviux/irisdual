
#include <atom/panic.hpp>
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

ARM64Backend::ARM64Backend(State& cpu_state)
    : m_cpu_state{cpu_state} {
}

void ARM64Backend::Execute(const ir::Function& function) {
  using namespace oaknut::util;

  static constexpr auto XReg_State = X0;

  oaknut::CodeBlock code_block{4096u};
  oaknut::CodeGenerator code{code_block.ptr()};

  const void* code_begin = code.xptr<void*>();
  code_block.unprotect();
  code.MOVP2R(XReg_State, &m_cpu_state);

  {
    const ir::BasicBlock& basic_block = function.basic_block;

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

    while(instruction != nullptr) {
      switch(instruction->type) {
        case ir::Instruction::Type::LDGPR: {
          const ir::GPR gpr = instruction->GetArg(0u).AsGPR();
          const oaknut::WReg result_reg = GetLocation(instruction->GetOut(0u)).AsWReg();
          code.LDR(result_reg, XReg_State, m_cpu_state.GetOffsetToGPR(gpr, CPU::Mode::Supervisor)); // TODO: cpu mode
          break;
        }
        case ir::Instruction::Type::STGPR: {
          const ir::GPR gpr = instruction->GetArg(0u).AsGPR();
          const oaknut::WReg value_reg = GetLocation(instruction->GetArg(1u).AsValue()).AsWReg(); // TODO: assumes that value is not a constant!
          code.STR(value_reg, XReg_State, m_cpu_state.GetOffsetToGPR(gpr, CPU::Mode::Supervisor)); // TODO: cpu mode
          break;
        }
        default: {
          ATOM_PANIC("unhandled IR instruction type: {}", (int)instruction->type);
        }
      }

      instruction = instruction->next;
    }


  }

  code.RET();
  code_block.protect();
  code_block.invalidate_all();

  fmt::print("===========================================\n");
  fmt::print("IR:\n{}\n", ir::disassemble(function.basic_block));
  fmt::print("ARM64:\n");
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

} // namespace dual::arm::jit

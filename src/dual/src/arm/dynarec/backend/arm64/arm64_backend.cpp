
#include <atom/panic.hpp>
#include <fmt/format.h>
#include <oaknut/code_block.hpp>
#include <oaknut/oaknut.hpp>
#include <capstone/capstone.h>

#include "arm64_backend.hpp"

namespace dual::arm::jit {

static void DisasA64(const void* code_begin, const void* code_end);

ARM64Backend::ARM64Backend(State& cpu_state)
    : m_cpu_state{cpu_state} {
}

void ARM64Backend::Execute(const ir::Function& function) {
  using namespace oaknut::util;

  oaknut::CodeBlock code_block{4096u};
  oaknut::CodeGenerator code{code_block.ptr()};

  const void* code_begin = code.xptr<void*>();
  code_block.unprotect();
  code.MOVP2R(X0, &m_cpu_state);
  code.LDR(W1, X0, m_cpu_state.GetOffsetToGPR(CPU::GPR::SP, CPU::Mode::Supervisor));
  code.ADD(W1, W1, 1);
  code.STR(W1, X0, m_cpu_state.GetOffsetToGPR(CPU::GPR::SP, CPU::Mode::Supervisor));
  code.RET();
  code_block.protect();
  code_block.invalidate_all();
  DisasA64(code_begin, code.xptr<void*>());

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

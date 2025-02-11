
#include <atom/panic.hpp>

#include "arm/interpreter/interpreter_cpu.hpp"
#include "backend/arm64/arm64_backend.hpp"
#include "ir/passes/guest_state_access_removal_pass.hpp"
#include "ir/passes/host_flag_propagation_pass.hpp"
#include "ir/passes/dead_code_removal_pass.hpp"
#include "dynarec_cpu.hpp"

namespace dual::arm {

DynarecCPU::DynarecCPU(
  Memory& memory,
  Scheduler& scheduler,
  CycleCounter& cycle_counter,
  Model model,
  std::span<const AttachCPn> coprocessors
)   : m_fallback_cpu{memory, scheduler, cycle_counter, model, coprocessors}
    , m_memory{memory} {
  m_backend = std::make_unique<jit::ARM64Backend>(m_cpu_state);
  m_ir_passes.push_back(std::make_unique<jit::ir::GuestStateAccessRemovalPass>());
  m_ir_passes.push_back(std::make_unique<jit::ir::HostFlagPropagationPass>());
  m_ir_passes.push_back(std::make_unique<jit::ir::DeadCodeRemovalPass>());
}

void DynarecCPU::Reset() {
  TestBackend();

  m_fallback_cpu.Reset();
  m_cpu_state.Reset();
}

u32  DynarecCPU::GetExceptionBase() const {
  return m_fallback_cpu.GetExceptionBase();
}

void DynarecCPU::SetExceptionBase(u32 address) {
  m_fallback_cpu.SetExceptionBase(address);
}

void DynarecCPU::InvalidateICache() {
}

void DynarecCPU::InvalidateICacheRange(u32 address_lo, u32 address_hi) {
}

void DynarecCPU::SetUnalignedDataAccessEnable(bool enable) {
  ATOM_PANIC("unimplemented");
}

bool DynarecCPU::GetWaitingForIRQ() const {
  return m_fallback_cpu.GetWaitingForIRQ();
}

void DynarecCPU::SetWaitingForIRQ(bool value) {
  m_fallback_cpu.SetWaitingForIRQ(value);
}

bool DynarecCPU::GetIRQFlag() const {
  return m_fallback_cpu.GetIRQFlag();
}

void DynarecCPU::SetIRQFlag(bool value) {
  m_fallback_cpu.SetIRQFlag(value);
}

u32 DynarecCPU::GetGPR(GPR reg) const {
  return m_cpu_state.GetGPR(reg);
}

u32 DynarecCPU::GetGPR(GPR reg, Mode mode) const {
  return m_cpu_state.GetGPR(reg, mode);
}

CPU::PSR DynarecCPU::GetCPSR() const {
  return m_cpu_state.GetCPSR();
}

CPU::PSR DynarecCPU::GetSPSR(Mode mode) const {
  return m_cpu_state.GetSPSR(mode);
}

void DynarecCPU::SetGPR(GPR reg, u32 value) {
  m_cpu_state.SetGPR(reg, value);
}

void DynarecCPU::SetGPR(GPR reg, Mode mode, u32 value) {
  m_cpu_state.SetGPR(reg, mode, value);
}

void DynarecCPU::SetCPSR(PSR value) {
  m_cpu_state.SetCPSR(value);
}

void DynarecCPU::SetSPSR(Mode mode, PSR value) {
  m_cpu_state.SetSPSR(mode, value);
}

void DynarecCPU::Run(int cycles) {
  const auto CopyB2A_Fast = [this]<class A, class B>(A& a, B& b) {
    a.SetCPSR(b.GetCPSR());

    u32 pc = b.GetGPR(GPR::R15);
    if(std::is_same_v<A, InterpreterCPU>) {
      // Well, this is slightly painful. Fix this properly.
      pc -= b.GetCPSR().thumb ? 4 : 8;
    }
    a.SetGPR(GPR::R15, pc);
  };

  const auto CopyB2A_Slow = [this]<class A, class B>(A& a, B& b) {
    a.SetSPSR(Mode::FIQ, b.GetSPSR(Mode::FIQ));
    a.SetSPSR(Mode::IRQ, b.GetSPSR(Mode::IRQ));
    a.SetSPSR(Mode::Supervisor, b.GetSPSR(Mode::Supervisor));
    a.SetSPSR(Mode::Abort, b.GetSPSR(Mode::Abort));
    a.SetSPSR(Mode::Undefined, b.GetSPSR(Mode::Undefined));

    a.SetGPR( GPR::R0, b.GetGPR( GPR::R0));
    a.SetGPR( GPR::R1, b.GetGPR( GPR::R1));
    a.SetGPR( GPR::R2, b.GetGPR( GPR::R2));
    a.SetGPR( GPR::R3, b.GetGPR( GPR::R3));
    a.SetGPR( GPR::R4, b.GetGPR( GPR::R4));
    a.SetGPR( GPR::R5, b.GetGPR( GPR::R5));
    a.SetGPR( GPR::R6, b.GetGPR( GPR::R6));
    a.SetGPR( GPR::R7, b.GetGPR( GPR::R7));
    a.SetGPR( GPR::R8, Mode::User, b.GetGPR( GPR::R8, Mode::User));
    a.SetGPR( GPR::R9, Mode::User, b.GetGPR( GPR::R9, Mode::User));
    a.SetGPR(GPR::R10, Mode::User, b.GetGPR(GPR::R10, Mode::User));
    a.SetGPR(GPR::R11, Mode::User, b.GetGPR(GPR::R11, Mode::User));
    a.SetGPR(GPR::R12, Mode::User, b.GetGPR(GPR::R12, Mode::User));
    a.SetGPR(GPR::R13, Mode::User, b.GetGPR(GPR::R13, Mode::User));
    a.SetGPR(GPR::R14, Mode::User, b.GetGPR(GPR::R14, Mode::User));
    a.SetGPR( GPR::R8, Mode::FIQ, b.GetGPR( GPR::R8, Mode::FIQ));
    a.SetGPR( GPR::R9, Mode::FIQ, b.GetGPR( GPR::R9, Mode::FIQ));
    a.SetGPR(GPR::R10, Mode::FIQ, b.GetGPR(GPR::R10, Mode::FIQ));
    a.SetGPR(GPR::R11, Mode::FIQ, b.GetGPR(GPR::R11, Mode::FIQ));
    a.SetGPR(GPR::R12, Mode::FIQ, b.GetGPR(GPR::R12, Mode::FIQ));
    a.SetGPR(GPR::R13, Mode::FIQ, b.GetGPR(GPR::R13, Mode::FIQ));
    a.SetGPR(GPR::R14, Mode::FIQ, b.GetGPR(GPR::R14, Mode::FIQ));
    a.SetGPR(GPR::R13, Mode::IRQ, b.GetGPR(GPR::R13, Mode::IRQ));
    a.SetGPR(GPR::R14, Mode::IRQ, b.GetGPR(GPR::R14, Mode::IRQ));
    a.SetGPR(GPR::R13, Mode::Supervisor, b.GetGPR(GPR::R13, Mode::Supervisor));
    a.SetGPR(GPR::R14, Mode::Supervisor, b.GetGPR(GPR::R14, Mode::Supervisor));
    a.SetGPR(GPR::R13, Mode::Abort, b.GetGPR(GPR::R13, Mode::Abort));
    a.SetGPR(GPR::R14, Mode::Abort, b.GetGPR(GPR::R14, Mode::Abort));
    a.SetGPR(GPR::R13, Mode::Undefined, b.GetGPR(GPR::R13, Mode::Undefined));
    a.SetGPR(GPR::R14, Mode::Undefined, b.GetGPR(GPR::R14, Mode::Undefined));
  };

  bool using_jit = true;

  for(int i = 0; i < cycles; i++) {
    jit::ir::Function* function = TryJit();

    if(function) {
      if(!using_jit) {
        CopyB2A_Slow(m_cpu_state, m_fallback_cpu);
        using_jit = true;
      }
      m_backend->Execute(*function, false);
    } else {
      CopyB2A_Fast(m_fallback_cpu, m_cpu_state);
      if(using_jit) {
        CopyB2A_Slow(m_fallback_cpu, m_cpu_state);
        using_jit = false;
      }
      m_fallback_cpu.Run(1);
      CopyB2A_Fast(m_cpu_state, m_fallback_cpu);
    }
  }

  if(!using_jit) {
    CopyB2A_Slow(m_cpu_state, m_fallback_cpu);
  }
}

jit::ir::Function* DynarecCPU::TryJit() {
  using namespace jit;

  static int stupid_counter = 0;

  const PSR cpsr = m_cpu_state.GetCPSR();

  if(cpsr.thumb == 0) {
    const u32 r15 = m_cpu_state.GetGPR(GPR::PC);
    const u32 instruction = m_memory.ReadWord(r15 - 8u, Memory::Bus::Code);
    const Mode cpu_mode = (Mode)cpsr.mode;

    if((stupid_counter++ % 10) == 0) {
      m_tmp_memory_arena.Reset();

      // TODO: implement ir::FunctionBuilder, or something like that.
      ir::Function* function = new(m_tmp_memory_arena.Allocate(sizeof(ir::Function))) ir::Function{}; // TODO: check failure
      ir::BasicBlock* bb = new(m_tmp_memory_arena.Allocate(sizeof(ir::BasicBlock))) ir::BasicBlock{0u}; // TODO: check failure
      function->basic_blocks.push_back(bb);

      ir::Emitter emitter{*bb, m_tmp_memory_arena};

      const TranslatorA32::Code code = m_translator_a32.Translate(r15, cpu_mode, instruction, emitter);
      if(code == TranslatorA32::Code::Success) {
        emitter.EXIT();
        OptimizeFunction(*function);
        return function;
      }
    }
  } else {
    // ...
  }

  return nullptr;
}

void DynarecCPU::TestBackend() {
  using namespace jit;

  ir::Function function{};

  ir::BasicBlock* bb_loop = new(m_tmp_memory_arena.Allocate(sizeof(ir::BasicBlock))) ir::BasicBlock{0u}; // TODO: check failure
  function.basic_blocks.push_back(bb_loop);

  ir::BasicBlock* bb_exit = new(m_tmp_memory_arena.Allocate(sizeof(ir::BasicBlock))) ir::BasicBlock{1u}; // TODO: check failure
  function.basic_blocks.push_back(bb_exit);

  {
    ir::Emitter emitter{*bb_loop, m_tmp_memory_arena};

//    const ir::U32Value& value_r0 = emitter.LDGPR(GPR::R0, Mode::User);
//    const ir::HostFlagsValue* hflags;
//    const ir::U32Value& result = emitter.ADD(value_r0, emitter.LDCONST(0xFFFFFFFFu), &hflags);
//    emitter.STGPR(GPR::R0, Mode::User, result);
//    emitter.STGPR(GPR::R8, Mode::User, emitter.LDGPR(GPR::R0, Mode::User));
//    emitter.STGPR(GPR::R0, Mode::User, emitter.LDGPR(GPR::R0, Mode::User));
//    emitter.STCPSR(emitter.LDCPSR());
//    emitter.STCPSR(emitter.LDCPSR());
//    emitter.STSPSR(Mode::IRQ, emitter.LDSPSR(Mode::IRQ));
//    emitter.STSPSR(Mode::FIQ, emitter.LDSPSR(Mode::FIQ));
//    emitter.STSPSR(Mode::IRQ, emitter.LDSPSR(Mode::IRQ));
//    emitter.STSPSR(Mode::FIQ, emitter.LDSPSR(Mode::FIQ));
//    emitter.BR_IF(ir::Condition::EQ, *hflags, *bb_exit, *bb_loop);

    const ir::HostFlagsValue* add_hflags;
    const ir::U32Value& add_result = emitter.ADD(emitter.LDGPR(GPR::R0, Mode::User), emitter.LDCONST(0xFFFFFFFFu), &add_hflags);
    emitter.STGPR(GPR::R0, Mode::User, add_result);

    const ir::U32Value& cpsr_old = emitter.LDCPSR();
    const ir::U32Value& nzcv_out = emitter.CVT_HFLAG_NZCV(*add_hflags);
    const ir::U32Value& cpsr_new = emitter.BITCMB(cpsr_old, nzcv_out, 0xF0000000u);
    emitter.STCPSR(cpsr_new);

    emitter.BR_IF(ir::Condition::EQ, emitter.CVT_NZCV_HFLAG(emitter.LDCPSR()), *bb_exit, *bb_loop);
  }

  {
    ir::Emitter emitter{*bb_exit, m_tmp_memory_arena};
    emitter.EXIT();
  }

  const auto PrintCpuState = [&]() {
    fmt::print("CPU STATE:\n");
    for(int reg = 0; reg < 16; reg++) {
      fmt::print("\tR{} \t= 0x{:08X}\n", reg, m_cpu_state.GetGPR((GPR)reg));
    }
    fmt::print("\tCPSR \t= 0x{:08X}\n", m_cpu_state.GetCPSR().word);
    fmt::print("\tSPSR \t= 0x{:08X}\n", m_cpu_state.GetSPSR((Mode)m_cpu_state.GetCPSR().mode).word);
  };
  m_cpu_state.Reset();
  m_cpu_state.SetGPR(GPR::R0, 256);
  PrintCpuState();
  OptimizeFunction(function);
  m_backend->Execute(function, true);
  PrintCpuState();

  m_tmp_memory_arena.Reset();
}

void DynarecCPU::OptimizeFunction(jit::ir::Function& function) {
  for(auto& pass : m_ir_passes) {
    pass->Run(function);
  }
}

} // namespace dual::arm

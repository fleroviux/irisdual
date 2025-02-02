
#include <atom/panic.hpp>

#include "arm/interpreter/interpreter_cpu.hpp"
#include "backend/arm64/arm64_backend.hpp"
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
}

void DynarecCPU::Reset() {
  m_fallback_cpu.Reset();
  m_cpu_state.Reset();
  TestBackend();
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
  return m_fallback_cpu.GetGPR(reg);
}

u32 DynarecCPU::GetGPR(GPR reg, Mode mode) const {
  return m_fallback_cpu.GetGPR(reg, mode);
}

CPU::PSR DynarecCPU::GetCPSR() const {
  return m_fallback_cpu.GetCPSR();
}

CPU::PSR DynarecCPU::GetSPSR(Mode mode) const {
  return m_fallback_cpu.GetSPSR(mode);
}

void DynarecCPU::SetGPR(GPR reg, u32 value) {
  m_fallback_cpu.SetGPR(reg, value);
}

void DynarecCPU::SetGPR(GPR reg, Mode mode, u32 value) {
  m_fallback_cpu.SetGPR(reg, mode, value);
}

void DynarecCPU::SetCPSR(PSR value) {
  m_fallback_cpu.SetCPSR(value);
}

void DynarecCPU::SetSPSR(Mode mode, PSR value) {
  m_fallback_cpu.SetSPSR(mode, value);
}

void DynarecCPU::Run(int cycles) {
  m_fallback_cpu.Run(cycles);
}

void DynarecCPU::TestBackend() {
  using namespace jit;

  atom::Arena memory_arena{16384u};

  ir::Function function{};

  /*ir::Emitter emitter{function.basic_blocks.emplace_back(), memory_arena};

  const ir::U32Value& value_r0 = emitter.LDGPR(GPR::R0, Mode::Supervisor);
  const ir::U32Value& value_r1 = emitter.LDGPR(GPR::R1, Mode::Supervisor);

  const ir::HostFlagsValue* value_hflags;
  const ir::U32Value& add_result = emitter.ADD(value_r0, value_r1, &value_hflags);
  emitter.STGPR(GPR::R2, Mode::Supervisor, add_result);

  // TODO: create a bitfield insert instruction in the IR?
  const ir::U32Value& value_nzcv = emitter.CVT_HFLAG_NZCV(*value_hflags);
  const ir::U32Value& value_cpsr_old = emitter.LDCPSR();
  const ir::U32Value& value_cpsr_new = emitter.ORR(value_nzcv, emitter.BIC(value_cpsr_old, emitter.LDCONST(0xF0000000u)));
  emitter.STCPSR(value_cpsr_new);

  emitter.EXIT();*/

  ir::BasicBlock* bb_loop = new(memory_arena.Allocate(sizeof(ir::BasicBlock))) ir::BasicBlock{0u}; // TODO: check failure
  function.basic_blocks.push_back(bb_loop);

  ir::BasicBlock* bb_exit = new(memory_arena.Allocate(sizeof(ir::BasicBlock))) ir::BasicBlock{1u}; // TODO: check failure
  function.basic_blocks.push_back(bb_exit);

  {
    ir::Emitter emitter{*bb_loop, memory_arena};

    const ir::U32Value& value_r0 = emitter.LDGPR(GPR::R0, Mode::Supervisor);

    const ir::HostFlagsValue* hflags;
    const ir::U32Value& result = emitter.ADD(value_r0, emitter.LDCONST(0xFFFFFFFFu), &hflags);
    emitter.STGPR(GPR::R0, Mode::Supervisor, result);
    emitter.BR_IF(ir::Condition::EQ, *hflags, *bb_exit, *bb_loop);
  }

  {
    ir::Emitter emitter{*bb_exit, memory_arena};
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
//  m_cpu_state.SetGPR(GPR::R0, 0x80000000);
//  m_cpu_state.SetGPR(GPR::R1, 0x80000000);
  m_cpu_state.SetGPR(GPR::R0, 256);
  PrintCpuState();
  m_backend->Execute(function);
  PrintCpuState();
}

} // namespace dual::arm

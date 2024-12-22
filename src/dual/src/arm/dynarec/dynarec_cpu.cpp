
#include <atom/panic.hpp>

#include "arm/interpreter/interpreter_cpu.hpp"
#include "dynarec_cpu.hpp"

namespace dual::arm {

DynarecCPU::DynarecCPU(
  Memory& memory,
  Scheduler& scheduler,
  CycleCounter& cycle_counter,
  Model model,
  std::span<const AttachCPn> coprocessors
)   : m_fallback_cpu{memory, scheduler, cycle_counter, model, coprocessors} {
}

void DynarecCPU::Reset() {
  m_fallback_cpu.Reset();
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

} // namespace dual::arm

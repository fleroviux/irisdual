
#include <atom/panic.hpp>

#include "dynarec_cpu.hpp"

namespace dual::arm {

DynarecCPU::DynarecCPU(
  Memory& memory,
  Scheduler& scheduler,
  CycleCounter& cycle_counter,
  Model model,
  std::span<const AttachCPn> coprocessors
) {
}

void DynarecCPU::Reset() {
  ATOM_PANIC("unimplemented");
}

u32  DynarecCPU::GetExceptionBase() const {
  ATOM_PANIC("unimplemented");
}

void DynarecCPU::SetExceptionBase(u32 address) {
  ATOM_PANIC("unimplemented");
}

void DynarecCPU::InvalidateICache() {
  ATOM_PANIC("unimplemented");
}

void DynarecCPU::InvalidateICacheRange(u32 address_lo, u32 address_hi) {
  ATOM_PANIC("unimplemented");
}

void DynarecCPU::SetUnalignedDataAccessEnable(bool enable) {
  ATOM_PANIC("unimplemented");
}

bool DynarecCPU::GetWaitingForIRQ() const {
  ATOM_PANIC("unimplemented");
}

void DynarecCPU::SetWaitingForIRQ(bool value) {
  ATOM_PANIC("unimplemented");
}

bool DynarecCPU::GetIRQFlag() const {
  ATOM_PANIC("unimplemented");
}

void DynarecCPU::SetIRQFlag(bool value) {
  ATOM_PANIC("unimplemented");
}

u32 DynarecCPU::GetGPR(GPR reg) const {
  ATOM_PANIC("unimplemented");
}

u32 DynarecCPU::GetGPR(GPR reg, Mode mode) const {
  ATOM_PANIC("unimplemented");
}

CPU::PSR DynarecCPU::GetCPSR() const {
  ATOM_PANIC("unimplemented");
}

CPU::PSR DynarecCPU::GetSPSR(Mode mode) const {
  ATOM_PANIC("unimplemented");
}

void DynarecCPU::SetGPR(GPR reg, u32 value) {
  ATOM_PANIC("unimplemented");
}

void DynarecCPU::SetGPR(GPR reg, Mode mode, u32 value) {
  ATOM_PANIC("unimplemented");
}

void DynarecCPU::SetCPSR(PSR value) {
  ATOM_PANIC("unimplemented");
}

void DynarecCPU::SetSPSR(Mode mode, PSR value) {
  ATOM_PANIC("unimplemented");
}

void DynarecCPU::Run(int cycles) {
  ATOM_PANIC("unimplemented");
}

} // namespace dual::arm

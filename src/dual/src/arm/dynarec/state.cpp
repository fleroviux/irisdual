
#include <utility>

#include "state.hpp"

namespace dual::arm::jit {

State::State() {
  BuildLUTs();
  Reset();
}

void State::Reset() {
  m_gpr_any.fill(0u);
  m_gpr_fiq.fill(0u);
  m_gpr_r13.fill(0u);
  m_gpr_r14.fill(0u);
  m_spsr.fill(PSR{0u});
  m_gpr_r15 = 0x00000008u;
  m_cpsr.word = (u32)Mode::Supervisor;
  m_cpsr.mask_fiq = 1u;
  m_cpsr.mask_irq = 1u;
}

void State::BuildLUTs() {
  constexpr std::pair<size_t, Mode> bank_and_mode_table[] = {
    { 0u, Mode::User       },
    { 1u, Mode::FIQ        },
    { 2u, Mode::IRQ        },
    { 3u, Mode::Supervisor },
    { 4u, Mode::Abort      },
    { 5u, Mode::Undefined  },
    { 0u, Mode::System     }
  };

  for(const auto [bank, mode] : bank_and_mode_table) {
    LUT& mode_lut = m_mode_luts[(int)mode];

    for(size_t reg = 0u; reg < 13u; reg++) {
      if(mode == Mode::FIQ && reg >= 8u) {
        mode_lut.gpr[reg] = &m_gpr_fiq[reg - 8u];
      } else {
        mode_lut.gpr[reg] = &m_gpr_any[reg];
      }
    }

    mode_lut.gpr[13] = &m_gpr_r13[bank];
    mode_lut.gpr[14] = &m_gpr_r14[bank];
    mode_lut.gpr[15] = &m_gpr_r15;
    mode_lut.spsr = &m_spsr[bank];
  }
}

u32 State::GetGPR(GPR gpr) const {
  return GetGPR(gpr, (Mode)m_cpsr.mode);
}

u32 State::GetGPR(GPR gpr, Mode mode) const {
  return *const_cast<State*>(this)->GetAddressOfGPR(gpr, mode);
}

State::PSR State::GetSPSR(Mode mode) const {
  return *const_cast<State*>(this)->GetAddressOfSPSR(mode);
}

State::PSR State::GetCPSR() const {
  return m_cpsr;
}

void State::SetGPR(GPR gpr, u32 value) {
  SetGPR(gpr, (Mode)m_cpsr.mode, value);
}

void State::SetGPR(GPR gpr, Mode mode, u32 value) {
  *GetAddressOfGPR(gpr, mode) = value;
}

void State::SetSPSR(Mode mode, PSR value) {
  *GetAddressOfSPSR(mode) = value;
}

void State::SetCPSR(PSR value) {
  m_cpsr = value;
}

std::ptrdiff_t State::GetOffsetToGPR(GPR gpr, Mode mode) {
  return (const u8*)GetAddressOfGPR(gpr, mode) - (const u8*)this;
}

std::ptrdiff_t State::GetOffsetToSPSR(Mode mode) {
  return (const u8*)GetAddressOfSPSR(mode) - (const u8*)this;
}

std::ptrdiff_t State::GetOffsetToCPSR() {
  return (const u8*)&m_cpsr - (const u8*)this;
}

u32* State::GetAddressOfGPR(GPR gpr, Mode mode) {
  const auto p_gpr = m_mode_luts[(int)mode].gpr[(int)gpr];
  if(p_gpr == nullptr) [[unlikely]] {
    ATOM_PANIC("invalid CPU mode: {}", (int)mode);
  }
  return p_gpr;
}

State::PSR* State::GetAddressOfSPSR(Mode mode) {
  const auto p_spsr = m_mode_luts[(int)mode].spsr;
  if(p_spsr == nullptr) [[unlikely]] {
    ATOM_PANIC("invalid CPU mode: {}", (int)mode);
  }
  return p_spsr;
}

} // namespace dual::arm::jit

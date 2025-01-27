
#pragma once

#include <array>
#include <atom/integer.hpp>
#include <atom/panic.hpp>
#include <dual/arm/cpu.hpp>
#include <cstddef>

namespace dual::arm::jit {

class State {
  public:
    using GPR  = CPU::GPR;
    using PSR  = CPU::PSR;
    using Mode = CPU::Mode;

    State();

    void Reset();

    [[nodiscard]] u32 GetGPR(GPR gpr) const;
    [[nodiscard]] u32 GetGPR(GPR gpr, Mode mode) const;
    [[nodiscard]] PSR GetSPSR(Mode mode) const;
    [[nodiscard]] PSR GetCPSR() const;

    void SetGPR(GPR gpr, u32 value);
    void SetGPR(GPR gpr, Mode mode, u32 value);
    void SetSPSR(Mode mode, PSR value);
    void SetCPSR(PSR value);

    std::ptrdiff_t GetOffsetToGPR(GPR gpr, Mode mode);
    std::ptrdiff_t GetOffsetToSPSR(Mode mode);
    std::ptrdiff_t GetOffsetToCPSR();

  private:
    struct LUT {
      u32* gpr[16]{};
      PSR* spsr{};
    };

    void BuildLUTs();
    [[nodiscard]] u32* GetAddressOfGPR(GPR gpr, Mode mode);
    [[nodiscard]] PSR* GetAddressOfSPSR(Mode mode);

    std::array<u32, 13> m_gpr_any{}; //< R0 - R12
    std::array<u32,  5> m_gpr_fiq{}; //< R8_fiq - R12_fiq
    std::array<u32,  6> m_gpr_r13{}; //< R13_{sys,fiq,irq,svc,abt,udf}
    std::array<u32,  6> m_gpr_r14{}; //< R14_{sys,fiq,irq,svc,abt,udf}
    std::array<PSR,  6> m_spsr{};    //< SPSR_{sys,fiq,irq,svc,abt,udf}
    u32 m_gpr_r15{}; //< Program Counter (R15)
    PSR m_cpsr{}; //< Current Program Status Register

    LUT m_mode_luts[0x20u]{}; //< Per ARM processor mode lookup table for GPRs and SPSRs.
};

} // namespace dual::arm::jit

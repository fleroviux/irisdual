
#pragma once

#include <dual/arm/cpu.hpp>
#include <dual/arm/memory.hpp>
#include <dual/common/scheduler.hpp>
#include <dual/common/cycle_counter.hpp>
#include <span>

#include "arm/interpreter/interpreter_cpu.hpp"

namespace dual::arm {

class DynarecCPU final : public CPU {
  public:
    DynarecCPU(
      Memory& memory,
      Scheduler& scheduler,
      CycleCounter& cycle_counter,
      Model model,
      std::span<const AttachCPn> coprocessors = {}
    );
    
    void Reset() override;

    u32  GetExceptionBase() const override;
    void SetExceptionBase(u32 address) override;

    void InvalidateICache() override;
    void InvalidateICacheRange(u32 address_lo, u32 address_hi) override;

    void SetUnalignedDataAccessEnable(bool enable) override;

    bool GetWaitingForIRQ() const override;
    void SetWaitingForIRQ(bool value) override;

    bool GetIRQFlag() const override;
    void SetIRQFlag(bool value) override;

    u32 GetGPR(GPR reg) const override;
    u32 GetGPR(GPR reg, Mode mode) const override;
    PSR GetCPSR() const override;
    PSR GetSPSR(Mode mode) const override;

    void SetGPR(GPR reg, u32 value) override;
    void SetGPR(GPR reg, Mode mode, u32 value) override;
    void SetCPSR(PSR value) override;
    void SetSPSR(Mode mode, PSR value) override;

    void Run(int cycles) override;

  private:
    InterpreterCPU m_fallback_cpu;
};

} // namespace dual::arm

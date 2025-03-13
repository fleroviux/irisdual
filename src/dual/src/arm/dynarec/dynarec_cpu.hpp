
#pragma once

#include <dual/arm/cpu.hpp>
#include <dual/arm/memory.hpp>
#include <dual/common/scheduler.hpp>
#include <dual/common/cycle_counter.hpp>
#include <memory>
#include <span>
#include <vector>

#include "arm/interpreter/interpreter_cpu.hpp"
#include "backend/backend.hpp"
#include "ir/emitter.hpp"
#include "ir/pass.hpp"
#include "state.hpp"
#include "translator_a32.hpp"
#include "translator_t16.hpp"

namespace dual::arm {

class DynarecCPU final : public CPU {
  public:
    DynarecCPU(
      Memory& memory,
      Scheduler& scheduler,
      CycleCounter& cycle_counter,
      Model model,
      std::span<const AttachCPn> coprocessor_table = {}
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
    jit::ir::Function* TryJit();

    void TestBackend();
    void OptimizeFunction(jit::ir::Function& function);

    InterpreterCPU m_fallback_cpu;
    jit::State m_cpu_state{};
    Memory& m_memory;
    Model m_cpu_model;
    std::unique_ptr<jit::Backend> m_backend{};

    jit::TranslatorA32 m_translator_a32{};
    jit::TranslatorT16 m_translator_t16;
    atom::Arena m_tmp_memory_arena{16384u};
    std::vector<std::unique_ptr<jit::ir::Pass>> m_ir_passes{};
};

} // namespace dual::arm

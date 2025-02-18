
#pragma once

#include <array>
#include <atom/logger/logger.hpp>
#include <atom/panic.hpp>
#include <dual/arm/coprocessor.hpp>
#include <dual/arm/cpu.hpp>
#include <dual/arm/memory.hpp>
#include <dual/common/cycle_counter.hpp>
#include <dual/common/scheduler.hpp>
#include <span>

namespace dual::arm {

  class InterpreterCPU final : public CPU {
    public:
      InterpreterCPU(
        Memory& memory,
        Scheduler& scheduler,
        CycleCounter& cycle_counter,
        Model model,
        std::span<const AttachCPn> coprocessor_table = {}
      );

      void Reset() override;

      u32 GetExceptionBase() const override {
        return m_exception_base;
      }

      void SetExceptionBase(u32 address) override {
        m_exception_base = address;
      }

      void SetUnalignedDataAccessEnable(bool enable) override {
        m_unaligned_data_access_enable = enable;
      }

      bool GetWaitingForIRQ() const override {
        return m_wait_for_irq;
      }

      void SetWaitingForIRQ(bool value) override {
        m_wait_for_irq = value;
      }

      bool GetIRQFlag() const override {
        return m_irq_line;
      }

      void SetIRQFlag(bool value) override {
        m_irq_line = value;
        m_wait_for_irq &= !value;
      }

      u32 GetGPR(GPR reg) const override {
        return m_state.reg[(int)reg];
      }

      u32 GetGPR(GPR reg, Mode mode) const override {
        const int max_shared_gpr = mode == Mode::FIQ ? 8 : 13;
        if((int)reg < max_shared_gpr || reg == GPR::PC || GetRegisterBankByMode(mode) == GetRegisterBankByMode((Mode)m_state.cpsr.mode)) {
          return m_state.reg[(int)reg];
        }
        return m_state.bank[(int)GetRegisterBankByMode(mode)][(int)reg - 8];
      }

      PSR GetCPSR() const override {
        return m_state.cpsr;
      }

      PSR GetSPSR(Mode mode) const override {
        return m_state.spsr[(int)GetRegisterBankByMode(mode)];
      }

      void SetGPR(GPR reg, u32 value) override {
        m_state.reg[(int)reg] = value;

        if(reg == GPR::PC) {
          ReloadPipelineOnSetR15OrCPSR();
        }
      }

      void SetGPR(GPR reg, Mode mode, u32 value) override {
        const int max_shared_gpr = mode == Mode::FIQ ? 8 : 13;
        if((int)reg < max_shared_gpr || reg == GPR::PC || GetRegisterBankByMode(mode) == GetRegisterBankByMode((Mode)m_state.cpsr.mode)) {
          SetGPR(reg, value);
        } else {
          m_state.bank[(int)GetRegisterBankByMode(mode)][(int)reg - 8] = value;
        }
      }

      void SetCPSR(PSR value) override {
        SwitchMode((Mode)value.mode);
        m_state.cpsr = value;
        ReloadPipelineOnSetR15OrCPSR();
      }

      void SetSPSR(Mode mode, PSR value) override {
        m_state.spsr[(int)GetRegisterBankByMode(mode)] = value;
      }

      void Run(int cycles) override;

      typedef void (InterpreterCPU::*Handler16)(u16);
      typedef void (InterpreterCPU::*Handler32)(u32);

    private:
      enum class Condition {
        EQ = 0,
        NE = 1,
        CS = 2,
        CC = 3,
        MI = 4,
        PL = 5,
        VS = 6,
        VC = 7,
        HI = 8,
        LS = 9,
        GE = 10,
        LT = 11,
        GT = 12,
        LE = 13,
        AL = 14,
        NV = 15
      };

      enum class Bank {
        None = 0,
        FIQ  = 1,
        Supervisor  = 2,
        Abort  = 3,
        IRQ  = 4,
        Undefined  = 5
      };

      friend struct TableGen;

      static auto GetRegisterBankByMode(Mode mode) -> Bank;

      void SignalIRQ();
      void ReloadPipeline16();
      void ReloadPipeline32();
      void ReloadPipelineOnSetR15OrCPSR();
      void BuildConditionTable();
      void SwitchMode(Mode new_mode);

      inline bool EvaluateCondition(Condition condition) {
        if(condition == Condition::AL) [[likely]] {
          return true;
        }
        return m_condition_table[(int)condition][m_state.cpsr.word >> 28];
      }

      #include "handlers/arithmetic.inl"
      #include "handlers/handler16.inl"
      #include "handlers/handler32.inl"
      #include "handlers/memory.inl"

      Memory& m_memory;
      Scheduler& m_scheduler;
      CycleCounter& m_cycle_counter;
      Model m_model;
      std::array<Coprocessor*, 16> m_coprocessors{};

      bool m_irq_line;
      bool m_wait_for_irq = false;
      u32 m_exception_base = 0;

      struct State {
        static constexpr int k_bank_count = 6;

        // General Purpose Registers
        union {
          struct {
            u32 r0;
            u32 r1;
            u32 r2;
            u32 r3;
            u32 r4;
            u32 r5;
            u32 r6;
            u32 r7;
            u32 r8;
            u32 r9;
            u32 r10;
            u32 r11;
            u32 r12;
            u32 r13;
            u32 r14;
            u32 r15;
          };
          u32 reg[16];
        };

        // Banked Registers
        u32 bank[k_bank_count][7];

        // Program Status Registers
        PSR cpsr;
        PSR spsr[k_bank_count];

        State() {
          for(int i = 0; i < 16; i++) {
            reg[i] = 0;
          }

          for(int b = 0; b < k_bank_count; b++) {
            for(int r = 0; r < 7; r++) {
              bank[b][r] = 0;
            }
            spsr[b] = 0;
          }

          cpsr.word = (uint)Mode::Supervisor;
          cpsr.mask_irq = 1;
          cpsr.mask_fiq = 1;
        }
      } m_state;

      PSR* m_spsr;

      u32 m_opcode[2];

      bool m_condition_table[16][16];

      static std::array<Handler16, 2048> k_opcode_lut_16;
      static std::array<Handler32, 8192> k_opcode_lut_32;

      bool m_unaligned_data_access_enable;
  };

} // namespace dual::arm

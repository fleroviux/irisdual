
#pragma once

#include <atom/panic.hpp>
#include <lunatic/cpu.hpp>
#include <dual/arm/cpu.hpp>
#include <dual/arm/memory.hpp>
#include <dual/arm/coprocessor.hpp>
#include <dual/common/cycle_counter.hpp>
#include <span>
#include <vector>

namespace dual::arm {

  class LunaticCPU final : public CPU {
    public:
      LunaticCPU(
        dual::arm::Memory& memory,
        CycleCounter& cycle_counter,
        Model model,
        std::span<const AttachCPn> coprocessor_table = {}
      )   : m_lunatic_memory{memory}
          , m_cycle_counter{cycle_counter} {
        lunatic::CPU::Descriptor::Model lunatic_cpu_model;
        std::array<lunatic::Coprocessor*, 16> lunatic_cop_array{};

        switch(model) {
          case Model::ARM7: lunatic_cpu_model = lunatic::CPU::Descriptor::Model::ARM7; break;
          case Model::ARM9: lunatic_cpu_model = lunatic::CPU::Descriptor::Model::ARM9; break;
          default: ATOM_PANIC("unimplemented CPU model");
        }

        for(auto& attach_cp_n : coprocessor_table) {
          dual::arm::Coprocessor& coprocessor = *attach_cp_n.coprocessor;
          m_lunatic_coprocessors.emplace_back(coprocessor);
          lunatic_cop_array.at(attach_cp_n.id) = &m_lunatic_coprocessors.back();
          coprocessor.SetCPU(this);
        }

        m_lunatic_cpu = lunatic::CreateCPU({
          .memory = m_lunatic_memory,
          .coprocessors = lunatic_cop_array,
          .model = lunatic_cpu_model
        });
      }

      void Reset() override {
        m_lunatic_cpu->Reset();
      }

      u32 GetExceptionBase() const override {
        return m_lunatic_cpu->GetExceptionBase();
      };

      void SetExceptionBase(u32 address) override {
        m_lunatic_cpu->SetExceptionBase(address);
      }

      void InvalidateICache() override {
        m_lunatic_cpu->ClearICache();
      }

      void InvalidateICacheRange(u32 address_lo, u32 address_hi) override {
        m_lunatic_cpu->ClearICacheRange(address_lo, address_hi);
      }

      void SetUnalignedDataAccessEnable(bool enable) override {
        ATOM_PANIC("unimplemented");
      }

      bool GetWaitingForIRQ() const override {
        return m_lunatic_cpu->WaitForIRQ();
      }

      void SetWaitingForIRQ(bool value) override {
        m_lunatic_cpu->WaitForIRQ() = value;
      }

      bool GetIRQFlag() const override {
        return m_lunatic_cpu->IRQLine();
      }

      void SetIRQFlag(bool value) override {
        m_lunatic_cpu->IRQLine() = value;
      }

      u32 GetGPR(GPR reg) const override {
        return m_lunatic_cpu->GetGPR(static_cast<lunatic::GPR>(reg));
      }

      u32 GetGPR(GPR reg, Mode mode) const override {
        return m_lunatic_cpu->GetGPR(static_cast<lunatic::GPR>(reg), static_cast<lunatic::Mode>(mode));
      }

      PSR GetCPSR() const override {
        return m_lunatic_cpu->GetCPSR().v;
      }

      PSR GetSPSR(Mode mode) const override {
        return m_lunatic_cpu->GetSPSR(static_cast<lunatic::Mode>(mode)).v;
      }

      void SetGPR(GPR reg, u32 value) override {
        m_lunatic_cpu->SetGPR(static_cast<lunatic::GPR>(reg), value);
      }

      void SetGPR(GPR reg, Mode mode, u32 value) override {
        m_lunatic_cpu->SetGPR(static_cast<lunatic::GPR>(reg), static_cast<lunatic::Mode>(mode), value);
      }

      void SetCPSR(PSR value) override {
        m_lunatic_cpu->SetCPSR({.v = value.word});
      }

      void SetSPSR(Mode mode, PSR value) override {
        m_lunatic_cpu->SetSPSR(static_cast<lunatic::Mode>(mode), {.v = value.word});
      }

      void Run(int cycles) override {
        const int actual_cycles = m_lunatic_cpu->Run(cycles);
        m_cycle_counter.AddDeviceCycles(actual_cycles);
      }

    private:
      struct Memory final : lunatic::Memory {
        explicit Memory(dual::arm::Memory& memory_impl) : m_memory_impl{memory_impl} {}

        u8 ReadByte(u32 address, Bus bus) override {
          return m_memory_impl.ReadByte(address, static_cast<dual::arm::Memory::Bus>(bus));
        }

        u16 ReadHalf(u32 address, Bus bus) override {
          return m_memory_impl.ReadHalf(address, static_cast<dual::arm::Memory::Bus>(bus));
        }

        u32 ReadWord(u32 address, Bus bus) override {
          return m_memory_impl.ReadWord(address, static_cast<dual::arm::Memory::Bus>(bus));
        }

        void WriteByte(u32 address, u8 value, Bus bus) override {
          m_memory_impl.WriteByte(address, value, static_cast<dual::arm::Memory::Bus>(bus));
        }

        void WriteHalf(u32 address, u16 value, Bus bus) override {
          m_memory_impl.WriteHalf(address, value, static_cast<dual::arm::Memory::Bus>(bus));
        }

        void WriteWord(u32 address, u32 value, Bus bus) override {
          m_memory_impl.WriteWord(address, value, static_cast<dual::arm::Memory::Bus>(bus));
        }

        dual::arm::Memory& m_memory_impl;
      };

      struct Coprocessor final : lunatic::Coprocessor {
        explicit Coprocessor(dual::arm::Coprocessor& coprocessor_impl) : m_coprocessor_impl{coprocessor_impl} {}

        bool ShouldWriteBreakBasicBlock(int opc1, int cn, int cm, int opc2) override {
          // @todo: evaluate if narrowing this down would have any real world benefits.
          return true;
        }

        u32 Read(int opc1, int cn, int cm, int opc2) override {
          return m_coprocessor_impl.MRC(opc1, cn, cm, opc2);
        }

        void Write(int opc1, int cn, int cm, int opc2, u32 value) override {
          m_coprocessor_impl.MCR(opc1, cn, cm, opc2, value);
        }

        dual::arm::Coprocessor& m_coprocessor_impl;
      };

      std::unique_ptr<lunatic::CPU> m_lunatic_cpu{};
      Memory m_lunatic_memory;
      std::vector<Coprocessor> m_lunatic_coprocessors{};
      CycleCounter& m_cycle_counter;
  };

} // namespace dual::arm

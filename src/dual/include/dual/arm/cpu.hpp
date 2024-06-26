
#pragma once

#include <atom/bit.hpp>
#include <atom/integer.hpp>
#include <dual/arm/coprocessor.hpp>

namespace dual::arm {

  struct AttachCPn {
    int id;
    dual::arm::Coprocessor* coprocessor;
  };

  class CPU {
    public:
      enum class Model {
        ARM7,
        ARM9,
        ARM11
      };

      enum class GPR {
        R0 = 0,
        R1 = 1,
        R2 = 2,
        R3 = 3,
        R4 = 4,
        R5 = 5,
        R6 = 6,
        R7 = 7,
        R8 = 8,
        R9 = 9,
        R10 = 10,
        R11 = 11,
        R12 = 12,
        R13 = 13,
        R14 = 14,
        R15 = 15,
        SP = 13,
        LR = 14,
        PC = 15
      };

      enum class Mode : uint {
        User = 0x10,
        FIQ = 0x11,
        IRQ = 0x12,
        Supervisor = 0x13,
        Abort = 0x17,
        Undefined = 0x1B,
        System = 0x1F
      };

      union PSR {
        atom::Bits< 0,  5, u32> mode;
        atom::Bits< 5,  1, u32> thumb;
        atom::Bits< 6,  1, u32> mask_fiq;
        atom::Bits< 7,  1, u32> mask_irq;
        atom::Bits< 8, 19, u32> reserved;
        atom::Bits<27,  1, u32> q;
        atom::Bits<28,  1, u32> v;
        atom::Bits<29,  1, u32> c;
        atom::Bits<30,  1, u32> z;
        atom::Bits<31,  1, u32> n;

        PSR() = default;
        PSR(u32 word) : word{word} {}

        u32 word = static_cast<u32>(Mode::Supervisor);
      };

      virtual ~CPU() = default;

      virtual void Reset() = 0;

      virtual u32  GetExceptionBase() const = 0;
      virtual void SetExceptionBase(u32 address) = 0;

      virtual void InvalidateICache() {}
      virtual void InvalidateICacheRange(u32 address_lo, u32 address_hi) {}

      virtual void SetUnalignedDataAccessEnable(bool enable) = 0;

      virtual bool GetWaitingForIRQ() const = 0;
      virtual void SetWaitingForIRQ(bool value) = 0;

      virtual bool GetIRQFlag() const = 0;
      virtual void SetIRQFlag(bool value) = 0;

      virtual u32 GetGPR(GPR reg) const = 0;
      virtual u32 GetGPR(GPR reg, Mode mode) const = 0;
      virtual PSR GetCPSR() const = 0;
      virtual PSR GetSPSR(Mode mode) const = 0;

      virtual void SetGPR(GPR reg, u32 value) = 0;
      virtual void SetGPR(GPR reg, Mode mode, u32 value) = 0;
      virtual void SetCPSR(PSR value) = 0;
      virtual void SetSPSR(Mode mode, PSR value) = 0;

      virtual void Run(int cycles) = 0;
  };

} // namespace dual::arm

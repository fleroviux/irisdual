
#pragma once

#include <atom/logger/logger.hpp>
#include <atom/bit.hpp>
#include <atom/integer.hpp>
#include <atom/panic.hpp>
#include <atom/punning.hpp>
#include <dual/arm/memory.hpp>
#include <dual/nds/ipc.hpp>
#include <dual/nds/system_memory.hpp>

namespace dual::nds::arm7 {

  class MemoryBus final : public dual::arm::Memory {
    public:
      struct HW {
        IPC& ipc;
      };

      MemoryBus(SystemMemory& memory, HW const& hw);

      u8  ReadByte(u32 address, Bus bus) override;
      u16 ReadHalf(u32 address, Bus bus) override;
      u32 ReadWord(u32 address, Bus bus) override;

      void WriteByte(u32 address, u8  value, Bus bus) override;
      void WriteHalf(u32 address, u16 value, Bus bus) override;
      void WriteWord(u32 address, u32 value, Bus bus) override;

    private:
      template<typename T> T    Read (u32 address, Bus bus);
      template<typename T> void Write(u32 address, T value, Bus bus);

      struct IO {
        u8  ReadByte(u32 address);
        u16 ReadHalf(u32 address);
        u32 ReadWord(u32 address);

        void WriteByte(u32 address, u8  value);
        void WriteHalf(u32 address, u16 value);
        void WriteWord(u32 address, u32 value);

        template<u32 mask> u32  ReadWord (u32 address);
        template<u32 mask> void WriteWord(u32 address, u32 value);

        HW hw;
      } m_io;

      u8* m_ewram;
      u8* m_iwram;
  };

} // namespace dual::nds::arm7
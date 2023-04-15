
#pragma once

#include <atom/logger/logger.hpp>
#include <atom/bit.hpp>
#include <atom/integer.hpp>
#include <atom/panic.hpp>
#include <atom/punning.hpp>
#include <dual/arm/memory.hpp>
#include <dual/nds/system_memory.hpp>

namespace dual::nds::arm9 {

  class MemoryBus final : public dual::arm::Memory {
    public:
      struct TCM {
        u8* data{};

        struct Config {
          bool readable = false;
          bool writable = false;
          u32 base_address = 0u;
          u32 high_address = 0u;
        } config{};
      };

      explicit MemoryBus(SystemMemory& memory);

      void SetupDTCM(TCM::Config const& config);
      void SetupITCM(TCM::Config const& config);

      u8  ReadByte(u32 address, Bus bus) override;
      u16 ReadHalf(u32 address, Bus bus) override;
      u32 ReadWord(u32 address, Bus bus) override;

      void WriteByte(u32 address, u8  value, Bus bus) override;
      void WriteHalf(u32 address, u16 value, Bus bus) override;
      void WriteWord(u32 address, u32 value, Bus bus) override;

    private:
      template<typename T> T    Read (u32 address, Bus bus);
      template<typename T> void Write(u32 address, T value, Bus bus);

      TCM m_dtcm{};
      TCM m_itcm{};

      u8* m_ewram;
  };

} // namespace dual::nds::arm9
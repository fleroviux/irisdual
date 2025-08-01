
#pragma once

#include <atom/logger/logger.hpp>
#include <atom/bit.hpp>
#include <atom/integer.hpp>
#include <atom/panic.hpp>
#include <atom/punning.hpp>
#include <dual/arm/memory.hpp>
#include <dual/nds/arm7/apu.hpp>
#include <dual/nds/arm7/dma.hpp>
#include <dual/nds/arm7/rtc.hpp>
#include <dual/nds/arm7/spi.hpp>
#include <dual/nds/arm7/wifi.hpp>
#include <dual/nds/video_unit/video_unit.hpp>
#include <dual/nds/vram/vram.hpp>
#include <dual/nds/cartridge.hpp>
#include <dual/nds/exmemcnt.hpp>
#include <dual/nds/irq.hpp>
#include <dual/nds/ipc.hpp>
#include <dual/nds/swram.hpp>
#include <dual/nds/system_memory.hpp>
#include <dual/nds/timer.hpp>

namespace dual::nds::arm7 {

  class MemoryBus final : public dual::arm::Memory {
    public:
      struct HW {
        IRQ& irq;
        Timer& timer;
        arm7::DMA& dma;
        arm7::SPI& spi;
        IPC& ipc;
        SWRAM& swram;
        VRAM& vram;
        VideoUnit& video_unit;
        Cartridge& cartridge;
        RTC& rtc;
        APU& apu;
        WIFI& wifi;
        u32& key_input;
        EXMEMCNT& exmemcnt;
      };

      MemoryBus(SystemMemory& memory, const HW& hw);

      void Reset();

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

        void Write_HALTCNT(u8 value);

        HW hw;
        u8 postflg{};
      } m_io;

      u8* m_boot_rom;
      u8* m_ewram;
      u8* m_iwram;
      SWRAM& m_swram;
      VRAM& m_vram;
  };

} // namespace dual::nds::arm7

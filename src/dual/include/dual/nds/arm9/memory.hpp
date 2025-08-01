
#pragma once

#include <atom/logger/logger.hpp>
#include <atom/bit.hpp>
#include <atom/integer.hpp>
#include <atom/panic.hpp>
#include <atom/punning.hpp>
#include <dual/arm/memory.hpp>
#include <dual/nds/video_unit/video_unit.hpp>
#include <dual/nds/vram/vram.hpp>
#include <dual/nds/arm9/math.hpp>
#include <dual/nds/arm9/dma.hpp>
#include <dual/nds/cartridge.hpp>
#include <dual/nds/exmemcnt.hpp>
#include <dual/nds/irq.hpp>
#include <dual/nds/ipc.hpp>
#include <dual/nds/swram.hpp>
#include <dual/nds/system_memory.hpp>
#include <dual/nds/timer.hpp>

namespace dual::nds::arm9 {

  class MemoryBus final : public dual::arm::Memory {
    public:
      struct HW {
        IRQ& irq;
        Timer& timer;
        arm9::DMA& dma;
        IPC& ipc;
        SWRAM& swram;
        VRAM& vram;
        Math& math;
        VideoUnit& video_unit;
        Cartridge& cartridge;
        u32& key_input;
        EXMEMCNT& exmemcnt;
      };

      struct TCM {
        u8* data{};

        struct Config {
          bool readable = false;
          bool writable = false;
          u32 base_address = 0u;
          u32 high_address = 0u;
        } config{};
      };

      MemoryBus(SystemMemory& memory, const HW& hw);

      void Reset();

      void SetupDTCM(const TCM::Config& config);
      void SetupITCM(const TCM::Config& config);

      u8  ReadByte(u32 address, Bus bus) override;
      u16 ReadHalf(u32 address, Bus bus) override;
      u32 ReadWord(u32 address, Bus bus) override;

      void WriteByte(u32 address, u8  value, Bus bus) override;
      void WriteHalf(u32 address, u16 value, Bus bus) override;
      void WriteWord(u32 address, u32 value, Bus bus) override;

    private:
      template<typename T> T    Read (u32 address, Bus bus);
      template<typename T> void Write(u32 address, T value, Bus bus);

      template<typename T>
      T ReadVRAM_PPU_BG(u32 address, int ppu_id) {
        return m_vram.region_ppu_bg[ppu_id].Read<T>(address & 0x1FFFFFu);
      }

      template<typename T>
      T ReadVRAM_PPU_OBJ(u32 address, int ppu_id) {
        return m_vram.region_ppu_obj[ppu_id].Read<T>(address & 0x1FFFFFu);
      }

      template<typename T>
      T ReadVRAM_LCDC(u32 address) {
        return m_vram.region_lcdc.Read<T>(address & 0xFFFFFu);
      }

      template<typename T>
      T ReadPRAM(u32 address) {
        const u32 offset = address & 0x7FFu;

        if(!m_io.hw.video_unit.GetPPU((int)(offset >> 10)).GetPowerOn()) [[unlikely]] {
          return 0u;
        }
        return atom::read<T>(m_pram, offset);
      }

      template<typename T>
      T ReadOAM(u32 address) {
        return atom::read<T>(m_oam, address & 0x7FFu);
      }

      template<typename T>
      void WriteVRAM_PPU_BG(u32 address, T value, int ppu_id) {
        const u32 offset = address & 0x1FFFFFu;

        m_vram.region_ppu_bg[ppu_id].Write<T>(offset, value);
        m_io.hw.video_unit.GetPPU(ppu_id).OnWriteVRAM_BG((size_t)offset, (size_t)offset + sizeof(T));
      }

      template<typename T>
      void WriteVRAM_PPU_OBJ(u32 address, T value, int ppu_id) {
        const u32 offset = address & 0x1FFFFFu;

        m_vram.region_ppu_obj[ppu_id].Write<T>(offset, value);
        m_io.hw.video_unit.GetPPU(ppu_id).OnWriteVRAM_OBJ((size_t)offset, (size_t)offset + sizeof(T));
      }

      template<typename T>
      void WriteVRAM_LCDC(u32 address, T value) {
        const u32 offset = address & 0xFFFFFu;

        m_vram.region_lcdc.Write<T>(offset, value);
        m_io.hw.video_unit.GetPPU(0).OnWriteVRAM_LCDC((size_t)offset, (size_t)offset + sizeof(T));
      }

      template<typename T>
      void WritePRAM(u32 address, T value) {
        const u32 offset = address & 0x7FFu;
        const int ppu_id = (int)(offset >> 10);
        PPU& ppu = m_io.hw.video_unit.GetPPU(ppu_id);

        if(ppu.GetPowerOn()) [[likely]] {
          atom::write<T>(m_pram, offset, value);
          ppu.OnWritePRAM(offset & 0x3FFu, (offset & 0x3FFu) + sizeof(T));
        }
      }

      template<typename T>
      void WriteOAM(u32 address, T value) {
        const u32 offset = address & 0x7FFu;
        const int ppu_id = (int)(offset >> 10);

        atom::write<T>(m_oam, offset, value);
        m_io.hw.video_unit.GetPPU(ppu_id).OnWriteOAM(offset & 0x3FFu, (offset & 0x3FFu) + sizeof(T));
      }

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

        u8 postflg{};
      } m_io;

      TCM m_dtcm{};
      TCM m_itcm{};

      u8* m_boot_rom;
      u8* m_ewram;
      u8* m_pram;
      u8* m_oam;
      SWRAM& m_swram;
      VRAM& m_vram;
  };

} // namespace dual::nds::arm9

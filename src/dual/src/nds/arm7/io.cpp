
#include <dual/nds/arm7/memory.hpp>

#define REG(address) ((address) >> 2)

#define APU_MAP_CHANNEL_WRITE_IO(channel) \
  case REG(0x04000400 | channel << 4): hw.apu.Write_SOUNDxCNT(channel, value, mask); break;\
  case REG(0x04000404 | channel << 4): hw.apu.Write_SOUNDxSAD(channel, value, mask); break;\
  case REG(0x04000408 | channel << 4): {\
    if(mask & 0x0000FFFFu) hw.apu.Write_SOUNDxTMR(channel, value, (u16)(mask >>  0));\
    if(mask & 0xFFFF0000u) hw.apu.Write_SOUNDxPNT(channel, value, (u16)(mask >> 16));\
    break;\
  }\
  case REG(0x0400040C | channel << 4): hw.apu.Write_SOUNDxLEN(channel, value, mask); break;

namespace dual::nds::arm7 {

  static constexpr int GetAccessSize(u32 mask) {
    int size = 0;
    u32 ff = 0xFFu;

    for(int i = 0; i < 4; i++) {
      if(mask & ff) size += 8;

      ff <<= 8;
    }

    return size;
  }

  static constexpr u32 GetAccessAddressOffset(u32 mask) {
    int offset = 0;

    for(int i = 0; i < 4; i++) {
      if(mask & 0xFFu) break;

      offset++;
      mask >>= 8;
    }

    return offset;
  }

  u8 MemoryBus::IO::ReadByte(u32 address) {
    switch(address & 3u) {
      case 0u: return ReadWord<0x000000FF>(address);;
      case 1u: return ReadWord<0x0000FF00>(address & ~3u) >>  8;
      case 2u: return ReadWord<0x00FF0000>(address & ~3u) >> 16;
      case 3u: return ReadWord<0xFF000000>(address & ~3u) >> 24;
    }
    return 0u;
  }

  u16 MemoryBus::IO::ReadHalf(u32 address) {
    switch(address & 2u) {
      case 0u: return ReadWord<0x0000FFFF>(address);
      case 2u: return ReadWord<0xFFFF0000>(address & ~2u) >> 16;
    }
    return 0u;
  }

  u32 MemoryBus::IO::ReadWord(u32 address) {
    return ReadWord<0xFFFFFFFF>(address);
  }

  void MemoryBus::IO::WriteByte(u32 address, u8  value) {
    const u32 word = value * 0x01010101u;

    switch(address & 3u) {
      case 0u: WriteWord<0x000000FF>(address      , word); break;
      case 1u: WriteWord<0x0000FF00>(address & ~3u, word); break;
      case 2u: WriteWord<0x00FF0000>(address & ~3u, word); break;
      case 3u: WriteWord<0xFF000000>(address & ~3u, word); break;
    }
  }

  void MemoryBus::IO::WriteHalf(u32 address, u16 value) {
    const u32 word = value * 0x00010001u;

    switch(address & 2u) {
      case 0u: WriteWord<0x0000FFFF>(address      , word); break;
      case 2u: WriteWord<0xFFFF0000>(address & ~2u, word); break;
    }
  }

  void MemoryBus::IO::WriteWord(u32 address, u32 value) {
    WriteWord<0xFFFFFFFF>(address, value);
  }

  template<u32 mask> u32 MemoryBus::IO::ReadWord(u32 address) {
    u32 value = 0u;

    const auto Unhandled = [&]() {
      const int access_size = GetAccessSize(mask);
      const u32 access_address = address + GetAccessAddressOffset(mask);

      ATOM_ERROR("arm7: IO: unhandled {}-bit read from 0x{:08X}", access_size, access_address);
    };

    switch(REG(address)) {
      // PPU
      case REG(0x04000004): {
        if(mask & 0x0000FFFFu) value |= hw.video_unit.Read_DISPSTAT(CPU::ARM7) << 0;
        if(mask & 0xFFFF0000u) value |= hw.video_unit.Read_VCOUNT() << 16;
        return value;
      }

      // DMA
      case REG(0x040000B0): return hw.dma.Read_DMASAD(0);
      case REG(0x040000B4): return hw.dma.Read_DMADAD(0);
      case REG(0x040000B8): return hw.dma.Read_DMACNT(0);
      case REG(0x040000BC): return hw.dma.Read_DMASAD(1);
      case REG(0x040000C0): return hw.dma.Read_DMADAD(1);
      case REG(0x040000C4): return hw.dma.Read_DMACNT(1);
      case REG(0x040000C8): return hw.dma.Read_DMASAD(2);
      case REG(0x040000CC): return hw.dma.Read_DMADAD(2);
      case REG(0x040000D0): return hw.dma.Read_DMACNT(2);
      case REG(0x040000D4): return hw.dma.Read_DMASAD(3);
      case REG(0x040000D8): return hw.dma.Read_DMADAD(3);
      case REG(0x040000DC): return hw.dma.Read_DMACNT(3);

      // Timer
      case REG(0x04000100): return hw.timer.Read_TMCNT(0);
      case REG(0x04000104): return hw.timer.Read_TMCNT(1);
      case REG(0x04000108): return hw.timer.Read_TMCNT(2);
      case REG(0x0400010C): return hw.timer.Read_TMCNT(3);

      // Key Input
      case REG(0x04000130): return (u16)hw.key_input;
      case REG(0x04000134): return hw.key_input & ~0xFFFFu;

      // RTC
      case REG(0x04000138): return hw.rtc.Read_RTC();

      // IPC
      case REG(0x04000180): return hw.ipc.Read_SYNC(CPU::ARM7);
      case REG(0x04000184): return hw.ipc.Read_FIFOCNT(CPU::ARM7);
      case REG(0x04100000): return hw.ipc.Read_FIFORECV(CPU::ARM7);

      // Cartridge interface (Slot 1)
      case REG(0x040001A0): {
        if(mask & 0x0000FFFFu) value |= hw.cartridge.Read_AUXSPICNT()  <<  0;
        if(mask & 0x00FF0000u) value |= hw.cartridge.Read_AUXSPIDATA() << 16;
        return value;
      }
      case REG(0x040001A4): return hw.cartridge.Read_ROMCTRL();
      case REG(0x040001A8): return hw.cartridge.Read_CARDCMD() >>  0;
      case REG(0x040001AC): return hw.cartridge.Read_CARDCMD() >> 32;
      case REG(0x04100010): return hw.cartridge.Read_CARDDATA();

      // SPI
      case REG(0x040001C0): {
        if(mask & 0x0000FFFFu) value |= hw.spi.Read_SPICNT()  <<  0;
        if(mask & 0xFFFF0000u) value |= hw.spi.Read_SPIDATA() << 16;
        return value;
      }

      // Memory Control
      case REG(0x04000204): return hw.exmemcnt.ReadHalf();

      // IRQ
      case REG(0x04000208): return hw.irq.Read_IME();
      case REG(0x04000210): return hw.irq.Read_IE();
      case REG(0x04000214): return hw.irq.Read_IF();

      // VRAMSTAT and WRAMSTAT
      case REG(0x04000240): {
        if(mask & 0x000000FFu) value |= hw.vram.Read_VRAMSTAT() << 0;
        if(mask & 0x0000FF00u) value |= hw.swram.Read_WRAMCNT() << 8;
        return value;
      }

      // POSTFLG and HALTCNT
      case REG(0x04000300): return postflg;

      // Sound
      case REG(0x04000400): return hw.apu.Read_SOUNDxCNT(0);
      case REG(0x04000410): return hw.apu.Read_SOUNDxCNT(1);
      case REG(0x04000420): return hw.apu.Read_SOUNDxCNT(2);
      case REG(0x04000430): return hw.apu.Read_SOUNDxCNT(3);
      case REG(0x04000440): return hw.apu.Read_SOUNDxCNT(4);
      case REG(0x04000450): return hw.apu.Read_SOUNDxCNT(5);
      case REG(0x04000460): return hw.apu.Read_SOUNDxCNT(6);
      case REG(0x04000470): return hw.apu.Read_SOUNDxCNT(7);
      case REG(0x04000480): return hw.apu.Read_SOUNDxCNT(8);
      case REG(0x04000490): return hw.apu.Read_SOUNDxCNT(9);
      case REG(0x040004A0): return hw.apu.Read_SOUNDxCNT(10);
      case REG(0x040004B0): return hw.apu.Read_SOUNDxCNT(11);
      case REG(0x040004C0): return hw.apu.Read_SOUNDxCNT(12);
      case REG(0x040004D0): return hw.apu.Read_SOUNDxCNT(13);
      case REG(0x040004E0): return hw.apu.Read_SOUNDxCNT(14);
      case REG(0x040004F0): return hw.apu.Read_SOUNDxCNT(15);
      case REG(0x04000500): return hw.apu.Read_SOUNDCNT();
      case REG(0x04000504): return hw.apu.Read_SOUNDBIAS();

      default: {
        if(address >= 0x04804000u && address < 0x048082F8u) {
          return hw.wifi.Read_IO(address);
        }

        Unhandled();
      }
    }

    return 0;
  }

  template<u32 mask> void MemoryBus::IO::WriteWord(u32 address, u32 value) {
    const auto Unhandled = [=]() {
      const int access_size = GetAccessSize(mask);
      const u32 access_address = address + GetAccessAddressOffset(mask);
      const u32 access_value = (value & mask) >> (GetAccessAddressOffset(mask) * 8);

      ATOM_ERROR("arm7: IO: unhandled {}-bit write to 0x{:08X} = 0x{:08X}", access_size, access_address, access_value);
    };

    switch(REG(address)) {
      // PPU
      case REG(0x04000004): hw.video_unit.Write_DISPSTAT(CPU::ARM7, value, (u16)mask);

      // DMA
      case REG(0x040000B0): hw.dma.Write_DMASAD(0, value, mask); break;
      case REG(0x040000B4): hw.dma.Write_DMADAD(0, value, mask); break;
      case REG(0x040000B8): hw.dma.Write_DMACNT(0, value, mask); break;
      case REG(0x040000BC): hw.dma.Write_DMASAD(1, value, mask); break;
      case REG(0x040000C0): hw.dma.Write_DMADAD(1, value, mask); break;
      case REG(0x040000C4): hw.dma.Write_DMACNT(1, value, mask); break;
      case REG(0x040000C8): hw.dma.Write_DMASAD(2, value, mask); break;
      case REG(0x040000CC): hw.dma.Write_DMADAD(2, value, mask); break;
      case REG(0x040000D0): hw.dma.Write_DMACNT(2, value, mask); break;
      case REG(0x040000D4): hw.dma.Write_DMASAD(3, value, mask); break;
      case REG(0x040000D8): hw.dma.Write_DMADAD(3, value, mask); break;
      case REG(0x040000DC): hw.dma.Write_DMACNT(3, value, mask); break;

      // Timer
      case REG(0x04000100): hw.timer.Write_TMCNT(0, value, mask); break;
      case REG(0x04000104): hw.timer.Write_TMCNT(1, value, mask); break;
      case REG(0x04000108): hw.timer.Write_TMCNT(2, value, mask); break;
      case REG(0x0400010C): hw.timer.Write_TMCNT(3, value, mask); break;

      // RTC
      case REG(0x04000138): {
        if(mask & 0x000000FFu) hw.rtc.Write_RTC((u8)value);
        break;
      }

      // IPC
      case REG(0x04000180): hw.ipc.Write_SYNC(CPU::ARM7, value, mask); break;
      case REG(0x04000184): hw.ipc.Write_FIFOCNT(CPU::ARM7, value, mask); break;
      case REG(0x04000188): hw.ipc.Write_FIFOSEND(CPU::ARM7, value); break;

      // Cartridge interface (Slot 1)
      case REG(0x040001A0): {
        if(mask & 0x0000FFFFu) hw.cartridge.Write_AUXSPICNT((u16)value, (u16)mask);
        if(mask & 0x00FF0000u) hw.cartridge.Write_AUXSPIDATA((u8)(value >> 16));
        break;
      }
      case REG(0x040001A4): hw.cartridge.Write_ROMCTRL(value, mask); break;
      case REG(0x040001A8): hw.cartridge.Write_CARDCMD((u64)value <<  0, (u64)mask <<  0); break;
      case REG(0x040001AC): hw.cartridge.Write_CARDCMD((u64)value << 32, (u64)mask << 32); break;

      // SPI
      case REG(0x040001C0): {
        if(mask & 0x0000FFFFu) hw.spi.Write_SPICNT((u16)value, (u16)mask);
        if(mask & 0xFFFF0000u) hw.spi.Write_SPIDATA((u8)(value >> 16));
        break;
      }

      // IRQ
      case REG(0x04000208): hw.irq.Write_IME(value, mask); break;
      case REG(0x04000210): hw.irq.Write_IE(value, mask); break;
      case REG(0x04000214): hw.irq.Write_IF(value, mask); break;

      // POSTFLG and HALTCNT
      case REG(0x04000300): {
        if(mask & 0x000000FFu) postflg |= value & 1u;
        if(mask & 0x0000FF00u) Write_HALTCNT(value >> 8);
        break;
      }

      // Sound
      APU_MAP_CHANNEL_WRITE_IO(0)
      APU_MAP_CHANNEL_WRITE_IO(1)
      APU_MAP_CHANNEL_WRITE_IO(2)
      APU_MAP_CHANNEL_WRITE_IO(3)
      APU_MAP_CHANNEL_WRITE_IO(4)
      APU_MAP_CHANNEL_WRITE_IO(5)
      APU_MAP_CHANNEL_WRITE_IO(6)
      APU_MAP_CHANNEL_WRITE_IO(7)
      APU_MAP_CHANNEL_WRITE_IO(8)
      APU_MAP_CHANNEL_WRITE_IO(9)
      APU_MAP_CHANNEL_WRITE_IO(10)
      APU_MAP_CHANNEL_WRITE_IO(11)
      APU_MAP_CHANNEL_WRITE_IO(12)
      APU_MAP_CHANNEL_WRITE_IO(13)
      APU_MAP_CHANNEL_WRITE_IO(14)
      APU_MAP_CHANNEL_WRITE_IO(15)
      case REG(0x04000500):  hw.apu.Write_SOUNDCNT(value, mask); break;
      case REG(0x04000504): hw.apu.Write_SOUNDBIAS(value, mask); break;

      default: {
        if(address >= 0x04804000u && address < 0x048082F8u) {
          hw.wifi.Write_IO(address, value, mask);
          return;
        }

        Unhandled();
      }
    }
  }

  void MemoryBus::IO::Write_HALTCNT(u8 value) {
    switch(value & 0xC0u) {
      case 0x80u: hw.irq.GetCPU()->SetWaitingForIRQ(true); break;
      case 0x40u: ATOM_PANIC("arm7: IO: attempt to enter (unimplemented) GBA mode");
      case 0xC0u: ATOM_PANIC("arm7: IO: attempt to enter (unimplemented) sleep mode");
    }
  }

} // namespace dual::nds::arm7

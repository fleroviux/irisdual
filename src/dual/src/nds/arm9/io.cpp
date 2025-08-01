
#include <dual/nds/arm9/memory.hpp>

#define REG(address) ((address) >> 2)

#define PPU_READ_16__(ppu, reg) ppu.m_mmio.reg.ReadHalf()

#define PPU_READ_1616(ppu, reg_lo, reg_hi, mask) \
  (((mask) & 0x0000FFFFu ? (ppu.m_mmio.reg_lo.ReadHalf() <<  0) : 0u) |\
   ((mask) & 0xFFFF0000u ? (ppu.m_mmio.reg_hi.ReadHalf() << 16) : 0u))

#define PPU_READ_32(ppu, reg) ppu.m_mmio.reg.ReadWord()

#define PPU_WRITE_16__(ppu, reg, value, mask) ppu.m_mmio.reg.WriteHalf(value, (u16)mask);

#define PPU_WRITE_1616(ppu, reg_lo, reg_hi, value, mask) {\
  if(mask & 0x0000FFFFu) ppu.m_mmio.reg_lo.WriteHalf((u16)((value) >>  0), (u16)((mask) >>  0));\
  if(mask & 0xFFFF0000u) ppu.m_mmio.reg_hi.WriteHalf((u16)((value) >> 16), (u16)((mask) >> 16));\
}

#define PPU_WRITE_32(ppu, reg, value, mask) ppu.m_mmio.reg.WriteWord(value, mask);

namespace dual::nds::arm9 {

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

      ATOM_ERROR("arm9: IO: unhandled {}-bit read from 0x{:08X}", access_size, access_address);
    };

    auto& gpu = hw.video_unit.GetGPU();
    auto& ppu_a = hw.video_unit.GetPPU(0);
    auto& ppu_b = hw.video_unit.GetPPU(1);

    switch(REG(address)) {
      // PPU A, GPU DISP3DCNT
      case REG(0x04000000): return PPU_READ_32(ppu_a, dispcnt);
      case REG(0x04000004): {
        if(mask & 0x0000FFFFu) value |= hw.video_unit.Read_DISPSTAT(CPU::ARM9) << 0;
        if(mask & 0xFFFF0000u) value |= hw.video_unit.Read_VCOUNT() << 16;
        return value;
      }
      case REG(0x04000008): return PPU_READ_1616(ppu_a, bgcnt[0], bgcnt[1], mask);
      case REG(0x0400000C): return PPU_READ_1616(ppu_a, bgcnt[2], bgcnt[3], mask);
      case REG(0x04000048): return PPU_READ_1616(ppu_a, winin, winout, mask);
      case REG(0x04000050): return PPU_READ_1616(ppu_a, bldcnt, bldalpha, mask);
      case REG(0x04000060): return gpu.Read_DISP3DCNT();
      case REG(0x04000064): return hw.video_unit.Read_DISPCAPCNT();
      case REG(0x0400006C): return PPU_READ_16__(ppu_a, master_bright);

      // PPU B
      case REG(0x04001000): return PPU_READ_32(ppu_b, dispcnt);
      case REG(0x04001008): return PPU_READ_1616(ppu_b, bgcnt[0], bgcnt[1], mask);
      case REG(0x0400100C): return PPU_READ_1616(ppu_b, bgcnt[2], bgcnt[3], mask);
      case REG(0x04001048): return PPU_READ_1616(ppu_b, winin, winout, mask);
      case REG(0x04001050): return PPU_READ_1616(ppu_b, bldcnt, bldalpha, mask);
      case REG(0x0400106C): return PPU_READ_16__(ppu_b, master_bright);

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
      case REG(0x040000E0): return hw.dma.Read_DMAFILL(0);
      case REG(0x040000E4): return hw.dma.Read_DMAFILL(1);
      case REG(0x040000E8): return hw.dma.Read_DMAFILL(2);
      case REG(0x040000EC): return hw.dma.Read_DMAFILL(3);

      // Timer
      case REG(0x04000100): return hw.timer.Read_TMCNT(0);
      case REG(0x04000104): return hw.timer.Read_TMCNT(1);
      case REG(0x04000108): return hw.timer.Read_TMCNT(2);
      case REG(0x0400010C): return hw.timer.Read_TMCNT(3);

      // Key Input
      case REG(0x04000130): return (u16)hw.key_input;

      // IPC
      case REG(0x04000180): return hw.ipc.Read_SYNC(CPU::ARM9);
      case REG(0x04000184): return hw.ipc.Read_FIFOCNT(CPU::ARM9);
      case REG(0x04100000): return hw.ipc.Read_FIFORECV(CPU::ARM9);

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

      // Memory Control
      case REG(0x04000204): return hw.exmemcnt.ReadHalf();

      // IRQ
      case REG(0x04000208): return hw.irq.Read_IME();
      case REG(0x04000210): return hw.irq.Read_IE();
      case REG(0x04000214): return hw.irq.Read_IF();

      // VRAMCNT and WRAMCNT
      case REG(0x04000240): {
        if(mask & 0x000000FFu) value |= hw.vram.Read_VRAMCNT(VRAM::Bank::A) <<  0;
        if(mask & 0x0000FF00u) value |= hw.vram.Read_VRAMCNT(VRAM::Bank::B) <<  8;
        if(mask & 0x00FF0000u) value |= hw.vram.Read_VRAMCNT(VRAM::Bank::C) << 16;
        if(mask & 0xFF000000u) value |= hw.vram.Read_VRAMCNT(VRAM::Bank::D) << 24;
        return value;
      }
      case REG(0x04000244): {
        if(mask & 0x000000FFu) value |= hw.vram.Read_VRAMCNT(VRAM::Bank::E) <<  0;
        if(mask & 0x0000FF00u) value |= hw.vram.Read_VRAMCNT(VRAM::Bank::F) <<  8;
        if(mask & 0x00FF0000u) value |= hw.vram.Read_VRAMCNT(VRAM::Bank::G) << 16;
        if(mask & 0xFF000000u) value |= hw.swram.Read_WRAMCNT() << 24;
        return value;
      }
      case REG(0x04000248): {
        if(mask & 0x000000FFu) value |= hw.vram.Read_VRAMCNT(VRAM::Bank::H) << 0;
        if(mask & 0x0000FF00u) value |= hw.vram.Read_VRAMCNT(VRAM::Bank::I) << 8;
        return value;
      }

      // ARM9 Math
      case REG(0x04000280): return hw.math.Read_DIVCNT();
      case REG(0x04000290): return hw.math.Read_DIV_NUMER() >>  0;
      case REG(0x04000294): return hw.math.Read_DIV_NUMER() >> 32;
      case REG(0x04000298): return hw.math.Read_DIV_DENOM() >>  0;
      case REG(0x0400029C): return hw.math.Read_DIV_DENOM() >> 32;
      case REG(0x040002A0): return hw.math.Read_DIV_RESULT() >>  0;
      case REG(0x040002A4): return hw.math.Read_DIV_RESULT() >> 32;
      case REG(0x040002A8): return hw.math.Read_DIVREM_RESULT() >>  0;
      case REG(0x040002AC): return hw.math.Read_DIVREM_RESULT() >> 32;
      case REG(0x040002B0): return hw.math.Read_SQRTCNT();
      case REG(0x040002B4): return hw.math.Read_SQRT_RESULT();
      case REG(0x040002B8): return hw.math.Read_SQRT_PARAM() >>  0;
      case REG(0x040002BC): return hw.math.Read_SQRT_PARAM() >> 32;

      // System and power control
      case REG(0x04000300): return postflg;
      case REG(0x04000304): return hw.video_unit.Read_POWCNT1();

      // GPU3D
      case REG(0x04000600): return gpu.Read_GXSTAT();
      case REG(0x04000604): return gpu.Read_RAM_COUNT();
      case REG(0x04000620) ... REG(0x0400062C): ATOM_PANIC("gpu: Unhandled read from POS_RESULT");
      case REG(0x04000630) ... REG(0x04000634): ATOM_PANIC("gpu: Unhandled read from VEC_RESULT");
      case REG(0x04000640) ... REG(0x0400067C): return (u32)gpu.GetClipMatrix()[(address >> 4) & 3][(address >> 2) & 3].Raw();
      case REG(0x04000680) ... REG(0x040006A0): return  (u32)gpu.GetVecMatrix()[(address >> 4) & 3][(address >> 2) & 3].Raw();

      default: {
        Unhandled();
      }
    }

    return 0;
  }

  template<u32 mask> void MemoryBus::IO::WriteWord(u32 address, u32 value) {
    const auto Unhandled = [&]() {
      const int access_size = GetAccessSize(mask);
      const u32 access_address = address + GetAccessAddressOffset(mask);
      const u32 access_value = (value & mask) >> (GetAccessAddressOffset(mask) * 8);

      ATOM_ERROR("arm9: IO: unhandled {}-bit write to 0x{:08X} = 0x{:08X}", access_size, access_address, access_value);
    };

    auto& gpu = hw.video_unit.GetGPU();

    auto& ppu_a = hw.video_unit.GetPPU(0);
    auto& ppu_b = hw.video_unit.GetPPU(1);

    switch(REG(address)) {
      // PPU A, GPU DISP3DCNT
      case REG(0x04000000): PPU_WRITE_32  (ppu_a, dispcnt, value, mask); break;
      case REG(0x04000004): hw.video_unit.Write_DISPSTAT(CPU::ARM9, value, (u16)mask); break;
      case REG(0x04000008): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, bgcnt[0], bgcnt[1], value, mask); break;
      case REG(0x0400000C): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, bgcnt[2], bgcnt[3], value, mask); break;
      case REG(0x04000010): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, bghofs[0], bgvofs[0], value, mask); break;
      case REG(0x04000014): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, bghofs[1], bgvofs[1], value, mask); break;
      case REG(0x04000018): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, bghofs[2], bgvofs[2], value, mask); break;
      case REG(0x0400001C): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, bghofs[3], bgvofs[3], value, mask); break;
      case REG(0x04000020): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, bgpa[0], bgpb[0], value, mask); break;
      case REG(0x04000024): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, bgpc[0], bgpd[0], value, mask); break;
      case REG(0x04000028): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_32  (ppu_a, bgx[0], value, mask); break;
      case REG(0x0400002C): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_32  (ppu_a, bgy[0], value, mask); break;
      case REG(0x04000030): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, bgpa[1], bgpb[1], value, mask); break;
      case REG(0x04000034): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, bgpc[1], bgpd[1], value, mask); break;
      case REG(0x04000038): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_32  (ppu_a, bgx[1], value, mask); break;
      case REG(0x0400003C): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_32  (ppu_a, bgy[1], value, mask); break;
      case REG(0x04000040): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, winh[0], winh[1], value, mask); break;
      case REG(0x04000044): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, winv[0], winv[1], value, mask); break;
      case REG(0x04000048): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, winin, winout, value, mask); break;
      case REG(0x0400004C): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_16__(ppu_a, mosaic, value, mask); break;
      case REG(0x04000050): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_a, bldcnt, bldalpha, value, mask); break;
      case REG(0x04000054): if(ppu_a.GetPowerOn()) [[likely]] PPU_WRITE_16__(ppu_a, bldy, value, mask); break;
      case REG(0x04000060): gpu.Write_DISP3DCNT((u16)value, (u16)mask); break;
      case REG(0x04000064): hw.video_unit.Write_DISPCAPCNT(value, mask); break;
      case REG(0x0400006C): PPU_WRITE_16__(ppu_a, master_bright, value, mask); break;

      // PPU B
      case REG(0x04001000): PPU_WRITE_32  (ppu_b, dispcnt, value, mask); break;
      case REG(0x04001008): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, bgcnt[0], bgcnt[1], value, mask); break;
      case REG(0x0400100C): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, bgcnt[2], bgcnt[3], value, mask); break;
      case REG(0x04001010): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, bghofs[0], bgvofs[0], value, mask); break;
      case REG(0x04001014): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, bghofs[1], bgvofs[1], value, mask); break;
      case REG(0x04001018): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, bghofs[2], bgvofs[2], value, mask); break;
      case REG(0x0400101C): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, bghofs[3], bgvofs[3], value, mask); break;
      case REG(0x04001020): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, bgpa[0], bgpb[0], value, mask); break;
      case REG(0x04001024): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, bgpc[0], bgpd[0], value, mask); break;
      case REG(0x04001028): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_32  (ppu_b, bgx[0], value, mask); break;
      case REG(0x0400102C): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_32  (ppu_b, bgy[0], value, mask); break;
      case REG(0x04001030): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, bgpa[1], bgpb[1], value, mask); break;
      case REG(0x04001034): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, bgpc[1], bgpd[1], value, mask); break;
      case REG(0x04001038): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_32  (ppu_b, bgx[1], value, mask); break;
      case REG(0x0400103C): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_32  (ppu_b, bgy[1], value, mask); break;
      case REG(0x04001040): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, winh[0], winh[1], value, mask); break;
      case REG(0x04001044): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, winv[0], winv[1], value, mask); break;
      case REG(0x04001048): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, winin, winout, value, mask); break;
      case REG(0x0400104C): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_16__(ppu_b, mosaic, value, mask); break;
      case REG(0x04001050): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_1616(ppu_b, bldcnt, bldalpha, value, mask); break;
      case REG(0x04001054): if(ppu_b.GetPowerOn()) [[likely]] PPU_WRITE_16__(ppu_b, bldy, value, mask); break;
      case REG(0x0400106C): PPU_WRITE_16__(ppu_b, master_bright, value, mask); break;

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
      case REG(0x040000E0): hw.dma.Write_DMAFILL(0, value, mask); break;
      case REG(0x040000E4): hw.dma.Write_DMAFILL(1, value, mask); break;
      case REG(0x040000E8): hw.dma.Write_DMAFILL(2, value, mask); break;
      case REG(0x040000EC): hw.dma.Write_DMAFILL(3, value, mask); break;

      // Timer
      case REG(0x04000100): hw.timer.Write_TMCNT(0, value, mask); break;
      case REG(0x04000104): hw.timer.Write_TMCNT(1, value, mask); break;
      case REG(0x04000108): hw.timer.Write_TMCNT(2, value, mask); break;
      case REG(0x0400010C): hw.timer.Write_TMCNT(3, value, mask); break;

      // IPC
      case REG(0x04000180): hw.ipc.Write_SYNC(CPU::ARM9, value, mask); break;
      case REG(0x04000184): hw.ipc.Write_FIFOCNT(CPU::ARM9, value, mask); break;
      case REG(0x04000188): hw.ipc.Write_FIFOSEND(CPU::ARM9, value); break;

      // Cartridge interface (Slot 1)
      case REG(0x040001A0): {
        if(mask & 0x0000FFFFu) hw.cartridge.Write_AUXSPICNT((u16)value, (u16)mask);
        if(mask & 0x00FF0000u) hw.cartridge.Write_AUXSPIDATA((u8)(value >> 16));
        break;
      }
      case REG(0x040001A4): hw.cartridge.Write_ROMCTRL(value, mask); break;
      case REG(0x040001A8): hw.cartridge.Write_CARDCMD((u64)value <<  0, (u64)mask <<  0); break;
      case REG(0x040001AC): hw.cartridge.Write_CARDCMD((u64)value << 32, (u64)mask << 32); break;

      // Memory Control
      case REG(0x04000204): hw.exmemcnt.WriteHalf(value, (u16)mask); break;

      // IRQ
      case REG(0x04000208): hw.irq.Write_IME(value, mask); break;
      case REG(0x04000210): hw.irq.Write_IE(value, mask); break;
      case REG(0x04000214): hw.irq.Write_IF(value, mask); break;

      // VRAMCNT and WRAMCNT
      case REG(0x04000240): {
        if(mask & 0x000000FFu) hw.vram.Write_VRAMCNT(VRAM::Bank::A, value >>  0);
        if(mask & 0x0000FF00u) hw.vram.Write_VRAMCNT(VRAM::Bank::B, value >>  8);
        if(mask & 0x00FF0000u) hw.vram.Write_VRAMCNT(VRAM::Bank::C, value >> 16);
        if(mask & 0xFF000000u) hw.vram.Write_VRAMCNT(VRAM::Bank::D, value >> 24);
        break;
      }
      case REG(0x04000244): {
        if(mask & 0x000000FFu) hw.vram.Write_VRAMCNT(VRAM::Bank::E, value >>  0);
        if(mask & 0x0000FF00u) hw.vram.Write_VRAMCNT(VRAM::Bank::F, value >>  8);
        if(mask & 0x00FF0000u) hw.vram.Write_VRAMCNT(VRAM::Bank::G, value >> 16);
        if(mask & 0xFF000000u) hw.swram.Write_WRAMCNT(value >> 24);
        break;
      }
      case REG(0x04000248): {
        if(mask & 0x000000FFu) hw.vram.Write_VRAMCNT(VRAM::Bank::H, value >> 0);
        if(mask & 0x0000FF00u) hw.vram.Write_VRAMCNT(VRAM::Bank::I, value >> 8);
        break;
      }

      // ARM9 Math
      case REG(0x04000280): hw.math.Write_DIVCNT(value, mask); break;
      case REG(0x04000290): hw.math.Write_DIV_NUMER((u64)value <<  0, (u64)mask <<  0); break;
      case REG(0x04000294): hw.math.Write_DIV_NUMER((u64)value << 32, (u64)mask << 32); break;
      case REG(0x04000298): hw.math.Write_DIV_DENOM((u64)value <<  0, (u64)mask <<  0); break;
      case REG(0x0400029C): hw.math.Write_DIV_DENOM((u64)value << 32, (u64)mask << 32); break;
      case REG(0x040002B0): hw.math.Write_SQRTCNT(value, mask); break;
      case REG(0x040002B8): hw.math.Write_SQRT_PARAM((u64)value <<  0, (u64)mask <<  0); break;
      case REG(0x040002BC): hw.math.Write_SQRT_PARAM((u64)value << 32, (u64)mask << 32); break;

      // System and power control
      case REG(0x04000300): postflg = (value & mask & 3u) | (postflg & ~(mask & 2u)); break;
      case REG(0x04000304): hw.video_unit.Write_POWCNT((u16)value, (u16)mask); break;

      // GPU3D
      case REG(0x04000330) ... REG(0x0400033C): {
        if(gpu.GetRenderEnginePowerOn()) [[likely]] {
          gpu.Write_EDGE_COLOR(address, value, mask);
        }
        break;
      }
      case REG(0x04000340): if(gpu.GetRenderEnginePowerOn()) [[likely]] gpu.Write_ALPHA_TEST_REF(value, mask); break;
      case REG(0x04000350): if(gpu.GetRenderEnginePowerOn()) [[likely]] gpu.Write_CLEAR_COLOR(value, mask); break;
      case REG(0x04000354): if(gpu.GetRenderEnginePowerOn()) [[likely]] gpu.Write_CLEAR_DEPTH(value, mask); break;
      case REG(0x04000358): if(gpu.GetRenderEnginePowerOn()) [[likely]] gpu.Write_FOG_COLOR(value, mask); break;
      case REG(0x0400035C): if(gpu.GetRenderEnginePowerOn()) [[likely]] gpu.Write_FOG_OFFSET(value, mask); break;
      case REG(0x04000360) ... REG(0x0400037C): {
        if(gpu.GetRenderEnginePowerOn()) [[likely]] {
          gpu.Write_FOG_TABLE(address, value, mask);
        }
        break;
      }
      case REG(0x04000380) ... REG(0x040003BC): {
        if(gpu.GetRenderEnginePowerOn()) [[likely]] {
          gpu.Write_TOON_TABLE(address, value, mask);
        }
        break;
      }
      case REG(0x04000400) ... REG(0x0400043C): {
        if(gpu.GetGeometryEnginePowerOn()) [[likely]] {
          gpu.Write_GXFIFO(value);
        }
        break;
      }
      case REG(0x04000440): // MTX_MODE
      case REG(0x04000444): // MTX_PUSH
      case REG(0x04000448): // MTX_POP
      case REG(0x0400044C): // MTX_STORE
      case REG(0x04000450): // MTX_RESTORE
      case REG(0x04000454): // MTX_IDENTITY
      case REG(0x04000458): // MTX_LOAD_4x4
      case REG(0x0400045C): // MTX_LOAD_4x3
      case REG(0x04000460): // MTX_MULT_4x4
      case REG(0x04000464): // MTX_MULT_4x3
      case REG(0x04000468): // MTX_MULT_3x3
      case REG(0x0400046C): // MTX_SCALE
      case REG(0x04000470): // MTX_TRANS
      case REG(0x04000480): // COLOR
      case REG(0x04000484): // NORMAL
      case REG(0x04000488): // TEXCOORD
      case REG(0x0400048C): // VTX_16
      case REG(0x04000490): // VTX_10
      case REG(0x04000494): // VTX_XY
      case REG(0x04000498): // VTX_XZ
      case REG(0x0400049C): // VTX_YZ
      case REG(0x040004A0): // VTX_DIFF
      case REG(0x040004A4): // POLYGON_ATTR
      case REG(0x040004A8): // TEXIMAGE_PARAM
      case REG(0x040004AC): // PLTT_BASE
      case REG(0x040004C0): // DIF_AMB
      case REG(0x040004C4): // SPE_EMI
      case REG(0x040004C8): // LIGHT_VECTOR
      case REG(0x040004CC): // LIGHT_COLOR
      case REG(0x040004D0): // SHININESS
      case REG(0x04000500): // BEGIN_VTXS
      case REG(0x04000504): // END_VTXS
      case REG(0x04000540): // SWAP_BUFFERS
      case REG(0x04000580): // VIEWPORT
      case REG(0x040005C0): // BOX_TEST
      case REG(0x040005C4): // POS_TEST
      case REG(0x040005C8): { // VEC_TEST
        if(gpu.GetGeometryEnginePowerOn()) [[likely]] {
          gpu.Write_GXCMDPORT(address, value);
        }
        break;
      }
      case REG(0x04000600): {
        if(gpu.GetRenderEnginePowerOn()) [[likely]] {
          gpu.Write_GXSTAT(value, mask);
        }
        break;
      }

      default: {
        Unhandled();
      }
    }
  }

} // namespace dual::nds::arm9

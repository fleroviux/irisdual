
#pragma once

#include <atom/bit.hpp>
#include <atom/integer.hpp>

namespace dual::nds {

struct EXMEMCNT {
  union {
    atom::Bits< 0, 2, u16> gba_slot_sram_access_time;
    atom::Bits< 2, 2, u16> gba_slot_rom_1st_access_time;
    atom::Bits< 4, 1, u16> gba_slot_rom_2nd_access_time;
    atom::Bits< 5, 2, u16> gba_slot_phi_pin_out;
    atom::Bits< 7, 1, u16> gba_slot_access_rights;
    atom::Bits<11, 1, u16> nds_slot_access_rights;
    atom::Bits<14, 1, u16> main_memory_interface_mode_switch;
    atom::Bits<15, 1, u16> main_memory_access_priority;

    u16 half{0x0000u};
  };

  u16 ReadHalf() {
    return half;
  }

  void WriteHalf(u16 value, u16 mask) {
    const u16 write_mask = mask & 0xC8FFu;

    half = (value & write_mask) | (half & ~write_mask);
  }
};

} // namespace dual::nds

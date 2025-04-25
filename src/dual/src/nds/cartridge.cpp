
#include <atom/logger/logger.hpp>
#include <dual/nds/cartridge.hpp>
#include <dual/nds/enums.hpp>
#include <fstream>

namespace dual::nds {

  static constexpr int k_cycles_per_byte[2] { 5, 8 };

  Cartridge::Cartridge(
    Scheduler& scheduler,
    IRQ& irq9,
    IRQ& irq7,
    arm9::DMA& dma9,
    arm7::DMA& dma7,
    SystemMemory& memory
  )   : m_scheduler{scheduler}, m_dma9{dma9}, m_dma7{dma7}, m_memory{memory} {
    m_irq[(int)CPU::ARM9] = &irq9;
    m_irq[(int)CPU::ARM7] = &irq7;
  }

  void Cartridge::Reset() {
    m_data_mode = DataMode::Unencrypted;
    m_auxspicnt = {};
    m_auxspidata = 0u;
    m_romctrl = {};
    m_cardcmd = {};
    m_transfer = {};
  }

  void Cartridge::DirectBoot() {
    m_data_mode = DataMode::MainDataLoad;
  }

  void Cartridge::SetROM(std::shared_ptr<ROM> rom, std::shared_ptr<arm7::SPI::Device> backup) {
    u32 game_id_code;
    rom->Read((u8*)&game_id_code, 12, sizeof(u32));
    InitKeyCode(game_id_code);

    const size_t rom_size = rom->Size();

    m_rom_mask = 0u;

    // Generate power-of-two mask for ROM mirroring.
    for(int i = 0; i < 32; i++) {
      if(rom_size <= (1 << i)) {
        m_rom_mask = (1 << i) - 1;
        break;
      }
    }

    m_rom = std::move(rom);
    m_backup = std::move(backup);
  }

  auto Cartridge::Read_AUXSPICNT() -> u16 {
    return m_auxspicnt.half;
  }

  void Cartridge::Write_AUXSPICNT(u16 value, u16 mask) {
    const u16 write_mask = 0xE0C3u & mask;

    m_auxspicnt.half = (value & write_mask) | (m_auxspicnt.half & ~write_mask);
  }

  auto Cartridge::Read_AUXSPIDATA() -> u8 {
    return m_auxspidata;
  }

  void Cartridge::Write_AUXSPIDATA(u8 value) {
    if(!m_backup) {
      m_auxspidata = 0xFFu;
      return;
    }

    m_backup->Select();
    m_auxspidata = m_backup->Transfer(value);
    if(!m_auxspicnt.chip_select_hold) {
      m_backup->Deselect();
    }
  }

  auto Cartridge::Read_ROMCTRL() -> u32 {
    return m_romctrl.word;
  }

  void Cartridge::Write_ROMCTRL(u32 value, u32 mask) {
    const u32 write_mask = 0x7FFFFFFF & mask;

    m_romctrl.word = (value & write_mask) | (m_romctrl.word & ~write_mask);

    if((value & mask & 0x80000000) && !m_romctrl.busy) {
      m_romctrl.busy = true;

      const int transfer_duration = k_cycles_per_byte[m_romctrl.transfer_clk_rate] * 8;

      m_scheduler.Add(transfer_duration, [this](int _) {
        HandleCommand();
      });
    }
  }

  auto Cartridge::Read_CARDCMD() -> u64 {
    return m_cardcmd.quad;
  }

  void Cartridge::Write_CARDCMD(u64 value, u64 mask) {
    m_cardcmd.quad = (value & mask) | (m_cardcmd.quad & ~mask);
  }

  auto Cartridge::Read_CARDDATA() -> u32 {
    u32 data = 0xFFFFFFFFu;

    if(!m_romctrl.data_ready) {
      return data;
    }

    if(m_transfer.data_count != 0) {
      data = m_transfer.data[m_transfer.index++ % m_transfer.data_count];
    } else {
      m_transfer.index++;
    }

    m_romctrl.data_ready = false;

    if(m_transfer.index == m_transfer.count) {
      m_romctrl.busy = false;
      m_transfer.index = 0;
      m_transfer.count = 0;

      if(m_auxspicnt.enable_transfer_ready_irq) {
        // @todo
        // if(exmemcnt.nds_slot_access == EXMEMCNT::CPU::ARM7) {
        //   irq7.Raise(IRQ::Source::Cart_DataReady);
        // } else {
        //   irq9.Raise(IRQ::Source::Cart_DataReady);
        //  }
        for(auto irq : m_irq) irq->Request(IRQ::Source::Cart_DataReady);
      }
    } else {
      m_scheduler.Add(k_cycles_per_byte[m_romctrl.transfer_clk_rate] * 4, [this](int _) {
        m_romctrl.data_ready = true;

        // if(exmemcnt.nds_slot_access == EXMEMCNT::CPU::ARM7) {
        //   dma7.Request(DMA7::Time::Slot1);
        // } else {
        //  dma9.Request(DMA9::Time::Slot1);
        //  }
        m_dma9.Request(arm9::DMA::StartTime::Slot1);
        m_dma7.Request(arm7::DMA::StartTime::Slot1);
      });
    }

    return data;
  }

  void Cartridge::HandleCommand() {
    const auto Unhandled = [this]() {
      const u8* cmd = m_cardcmd.byte;

      ATOM_PANIC(
        "slot1: unhandled command (mode={}): {:02X}-{:02X}-{:02X}-{:02X}-{:02X}-{:02X}-{:02X}-{:02X}",
         (int)m_data_mode, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7]
      );
    };

    m_transfer.index = 0;
    m_transfer.data_count = 0;
    m_romctrl.data_ready = false;

    if(!m_rom) {
      return;
    }

    const uint data_block_size = m_romctrl.data_block_size;

    if(data_block_size == 0) {
      m_transfer.count = 0;
    } else if(data_block_size == 7) {
      m_transfer.count = 1;
    } else {
      m_transfer.count = 64 << data_block_size;
    }

    if(m_data_mode == DataMode::Unencrypted) {
      switch(m_cardcmd.byte[0]) {
        case 0x9F: {
          // Dummy (read high-z bytes)
          m_transfer.data[0] = 0xFFFFFFFFu;
          m_transfer.data_count = 1;
          break;
        }
        case 0x00: {
          // Get Cartridge Header
          // @todo: check what the correct behavior for reading past 0x200 bytes is.
          m_rom->Read((u8*)m_transfer.data, 0, 0x200);
          std::memset(&m_transfer.data[0x80], 0xFF, 0xE00);
          m_transfer.data_count = 0x400;
          break;
        }
        case 0x90: {
          // 1st Get ROM Chip ID
          m_transfer.data[0] = k_chip_id;
          m_transfer.data_count = 1;
          break;
        }
        case 0x3C: {
          // Activate KEY1 encryption
          m_data_mode = DataMode::SecureAreaLoad;
          break;
        }
        default: {
          Unhandled();
          break;
        }
      }
    } else if(m_data_mode == DataMode::SecureAreaLoad) {
      u8 command[8];

      for(int i = 0; i < 8; i++) {
        command[i] = m_cardcmd.byte[7 - i];
      }

      Decrypt64(m_key1_buffer_lvl2, (u32*)&command[0]);

      for(int i = 0; i < 4; i++) {
        std::swap(command[i], command[7 - i]);
      }

      switch(command[0] & 0xF0) {
        case 0x40: {
          // Activate KEY2 Encryption Mode
          ATOM_WARN("slot1: unhandled 'Activate KEY2 encryption' command");
          break;
        }
        case 0x10: {
          // 2nd Get ROM Chip ID
           m_transfer.data[0] = k_chip_id;
          m_transfer.data_count = 1;
          break;
        }
        case 0x20: {
          // Get Secure Area Block
          u32 address = (command[2] & 0xF0) << 8;

          m_transfer.data_count = 1024;

          m_rom->Read((u8*)m_transfer.data, address, 4096);

          if(address == 0x4000) {
            m_transfer.data[0] = 0x72636e65; // encr
            m_transfer.data[1] = 0x6a624f79; // yObj

            for(int i = 0; i < 512; i += 2) {
              Encrypt64(m_key1_buffer_lvl3, &m_transfer.data[i]);
            }

            Encrypt64(m_key1_buffer_lvl2, &m_transfer.data[0]);
          }
          break;
        }
        case 0xA0: {
          // Enter Main Data Mode
          m_data_mode = DataMode::MainDataLoad;
          break;
        }
        default: {
          Unhandled();
          break;
        }
      }
    } else if(m_data_mode == DataMode::MainDataLoad) {
      switch(m_cardcmd.byte[0]) {
        case 0xB7: {
          // Get Data
          u32 address;

          address  = m_cardcmd.byte[1] << 24;
          address |= m_cardcmd.byte[2] << 16;
          address |= m_cardcmd.byte[3] <<  8;
          address |= m_cardcmd.byte[4] <<  0;

          address &= m_rom_mask;

          if(address <= 0x7FFF) {
            address = 0x8000 + (address & 0x1FF);
            ATOM_WARN("slot1: attempted to read protected region.");
          }

          if(m_transfer.count > 0x80) {
            ATOM_PANIC("slot1: command 0xB7: size greater than 0x200 is untested.");
          }

          m_transfer.data_count = std::min(0x80, m_transfer.count);

          const u32 byte_len = m_transfer.data_count * sizeof(u32);

          u32 sector_a = address >> 12;
          u32 sector_b = (address + byte_len - 1) >> 12;

          if(sector_a != sector_b) {
            u32 size_a = 0x1000 - (address & 0xFFF);
            u32 size_b = byte_len - size_a;

            m_rom->Read((u8*)m_transfer.data, address, size_a);
            m_rom->Read((u8*)m_transfer.data + size_a, address & ~0xFFF, size_b);
          } else {
            m_rom->Read((u8*)m_transfer.data, address, byte_len);
          }
          break;
        }
        case 0xB8: {
          // 3rd Get ROM Chip ID
          m_transfer.data[0] = k_chip_id;
          m_transfer.data_count = 1;
          break;
        }
        default: {
          Unhandled();
          break;
        }
      }
    }

    m_romctrl.busy = m_transfer.data_count != 0;

    if(m_romctrl.busy) {
      m_scheduler.Add(k_cycles_per_byte[m_romctrl.transfer_clk_rate] * 4, [this](int late) {
        m_romctrl.data_ready = true;

        // @todo
        // if(exmemcnt.nds_slot_access == EXMEMCNT::CPU::ARM7) {
        //   dma7.Request(DMA7::Time::Slot1);
        // } else {
        //   dma9.Request(DMA9::Time::Slot1);
        // }
        m_dma9.Request(arm9::DMA::StartTime::Slot1);
        m_dma7.Request(arm7::DMA::StartTime::Slot1);
      });
    } else if(m_auxspicnt.enable_transfer_ready_irq) {
      // @todo
      // if(exmemcnt.nds_slot_access == EXMEMCNT::CPU::ARM7) {
      //   irq7.Raise(IRQ::Source::Cart_DataReady);
      // } else {
      //   irq9.Raise(IRQ::Source::Cart_DataReady);
      // }
      for(auto irq : m_irq) irq->Request(IRQ::Source::Cart_DataReady);
    }
  }

  void Cartridge::Encrypt64(u32* key_buffer, u32* ptr) {
    u32 x = ptr[1];
    u32 y = ptr[0];

    for(int i = 0; i <= 0xF; i++) {
      u32 z = key_buffer[i] ^ x;

      x = key_buffer[0x012 + u8(z >> 24)];
      x = key_buffer[0x112 + u8(z >> 16)] + x;
      x = key_buffer[0x212 + u8(z >>  8)] ^ x;
      x = key_buffer[0x312 + u8(z >>  0)] + x;

      x ^= y;
      y  = z;
    }

    ptr[0] = x ^ key_buffer[16];
    ptr[1] = y ^ key_buffer[17];
  }

  void Cartridge::Decrypt64(u32* key_buffer, u32* ptr) {
    u32 x = ptr[1];
    u32 y = ptr[0];

    for(int i = 0x11; i >= 0x02; i--) {
      u32 z = key_buffer[i] ^ x;

      x = key_buffer[0x012 + u8(z >> 24)];
      x = key_buffer[0x112 + u8(z >> 16)] + x;
      x = key_buffer[0x212 + u8(z >>  8)] ^ x;
      x = key_buffer[0x312 + u8(z >>  0)] + x;

      x ^= y;
      y  = z;
    }

    ptr[0] = x ^ key_buffer[1];
    ptr[1] = y ^ key_buffer[0];
  }

  void Cartridge::InitKeyCode(u32 game_id_code) {
    u32 keycode[3];

    const auto apply_keycode = [&](u32* key_buffer_dst, u32* key_buffer_src) {
      u32 scratch[2] = {0, 0};

      Encrypt64(key_buffer_src, &keycode[1]);
      Encrypt64(key_buffer_src, &keycode[0]);

      for(int i = 0; i <= 0x11; i++) {
        // @todo: do not rely on built-ins
        key_buffer_dst[i] = key_buffer_src[i] ^ __builtin_bswap32(keycode[i & 1]);
      }

      for(int i = 0x12; i <= 0x411; i++) {
        key_buffer_dst[i] = key_buffer_src[i];
      }

      for(int i = 0; i <= 0x410; i += 2)  {
        Encrypt64(key_buffer_dst, scratch);
        key_buffer_dst[i + 0] = scratch[1];
        key_buffer_dst[i + 1] = scratch[0];
      }
    };

    std::memcpy(m_key1_buffer_lvl2, &m_memory.arm7.bios[0x30], 0x1048);

    keycode[0] = game_id_code;
    keycode[1] = game_id_code >> 1;
    keycode[2] = game_id_code << 1;
    apply_keycode(m_key1_buffer_lvl2, m_key1_buffer_lvl2);
    apply_keycode(m_key1_buffer_lvl2, m_key1_buffer_lvl2);

    keycode[1] <<= 1;
    keycode[2] >>= 1;
    apply_keycode(m_key1_buffer_lvl3, m_key1_buffer_lvl2);
  }

} // namespace dual::nds

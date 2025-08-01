
#pragma once

#include <atom/bit.hpp>
#include <atom/integer.hpp>
#include <dual/common/scheduler.hpp>
#include <dual/nds/arm7/dma.hpp>
#include <dual/nds/arm7/spi.hpp>
#include <dual/nds/arm9/dma.hpp>
#include <dual/nds/exmemcnt.hpp>
#include <dual/nds/irq.hpp>
#include <dual/nds/rom.hpp>
#include <dual/nds/system_memory.hpp>
#include <memory>

namespace dual::nds {

  class Cartridge {
    public:
      Cartridge(
        Scheduler& scheduler,
        IRQ& irq9,
        IRQ& irq7,
        arm9::DMA& dma9,
        arm7::DMA& dma7,
        SystemMemory& memory,
        EXMEMCNT& exmemcnt
      );

      void Reset();
      void DirectBoot();

      void SetROM(std::shared_ptr<ROM> rom);
      void SetBackup(std::shared_ptr<arm7::SPI::BackupDevice> backup);

      void SetBackupDeviceHint(arm7::SPI::BackupDevice::Type type, size_t byte_size);

      auto  Read_AUXSPICNT() -> u16;
      void Write_AUXSPICNT(u16 value, u16 mask);

      auto  Read_AUXSPIDATA() -> u8;
      void Write_AUXSPIDATA(u8 value);

      auto  Read_ROMCTRL() -> u32;
      void Write_ROMCTRL(u32 value, u32 mask);

      auto  Read_CARDCMD() -> u64;
      void Write_CARDCMD(u64 value, u64 mask);

      auto Read_CARDDATA() -> u32;

    private:
      // @todo: use a plausible chip ID based on the ROM size.
      static constexpr u32 k_chip_id = 0x1FC2u;

      void HandleCommand();

      void Encrypt64(u32* key_buffer, u32* ptr);
      void Decrypt64(u32* key_buffer, u32* ptr);
      void InitKeyCode(u32 game_id_code);

      enum class DataMode {
        Unencrypted,
        SecureAreaLoad,
        MainDataLoad
      } m_data_mode{};

      union AUXSPICNT {
        atom::Bits< 0, 2, u16> baud_rate;
        atom::Bits< 6, 1, u16> chip_select_hold;
        atom::Bits< 7, 1, u16> busy;
        atom::Bits<13, 1, u16> nds_slot_mode; // 0 = Parallel/ROM, 1 = Serial/SPI-Backup
        atom::Bits<14, 1, u16> enable_transfer_ready_irq;
        atom::Bits<15, 1, u16> enable_nds_slot;

        u16 half = 0u;
      } m_auxspicnt{};

      u8 m_auxspidata{};

      union ROMCTRL {
        atom::Bits<23, 1, u32> data_ready;
        atom::Bits<24, 3, u32> data_block_size;
        atom::Bits<27, 1, u32> transfer_clk_rate;
        atom::Bits<31, 1, u32> busy;

        u32 word = 0u;
      } m_romctrl{};

      union CARDCMD {
        u8  byte[8];
        u64 quad = 0u;
      } m_cardcmd{};

      struct TransferData {
        int index = 0;      //< Current index into the data buffer (before modulo data_count)
        int count = 0;      //< Number of requested words
        int data_count = 0; //< Number of available words
        u32 data[0x1000]{}; //< Underlying transfer buffer
      } m_transfer{};

      u32 m_key1_buffer_lvl2[0x412]{};
      u32 m_key1_buffer_lvl3[0x412]{};

      Scheduler& m_scheduler;
      IRQ* m_irq[2]{};
      arm9::DMA& m_dma9;
      arm7::DMA& m_dma7;
      SystemMemory& m_memory;
      EXMEMCNT& m_exmemcnt;
      std::shared_ptr<ROM> m_rom{};
      std::shared_ptr<arm7::SPI::BackupDevice> m_backup{};
      u32 m_rom_mask{};
  };

} // namespace dual::nds

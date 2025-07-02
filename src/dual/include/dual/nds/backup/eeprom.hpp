
#pragma once

#include <dual/common/backup_file.hpp>
#include <dual/nds/arm7/spi.hpp>

namespace dual::nds {

  // EEPROM (and FRAM) memory emulation
  class EEPROM final : public arm7::SPI::BackupDevice {
    public:
      enum class Size {
        _8K,
        _32K,
        _64K,
        _128K
      };

      EEPROM(const std::string& save_path, Size size_hint, bool fram);

      void Reset() override;
      void Select() override;
      void Deselect() override;

      u8 Transfer(u8 data) override;

    private:
      enum class Command : u8 {
        WriteEnable  = 0x06, // WREM
        WriteDisable = 0x04, // WRDI
        ReadStatus   = 0x05, // RDSR
        WriteStatus  = 0x01, // WRSR
        Read         = 0x03, // RD
        Write        = 0x02  // WR
      };

      enum class State {
        Idle,
        Deselected,
        ReceiveCommand,
        ReadStatus,
        WriteStatus,
        ReadAddress0,
        ReadAddress1,
        ReadAddress2,
        Read,
        Write
      } m_state{};

      void ParseCommand(Command cmd);

      Command m_current_cmd{};
      u32 m_address{};
      bool m_write_enable_latch{};
      int  m_write_protect_mode{};
      bool m_status_reg_write_disable{};

      std::string m_save_path{};
      Size m_size_hint{};
      Size m_size{};
      bool m_fram{};
      size_t m_mask{};
      size_t m_page_mask{};
      u32 m_address_upper_half{};
      u32 m_address_upper_quarter{};

      std::unique_ptr<BackupFile> m_file;
  };

} // namespace dual::nds


#pragma once

#include <dual/common/backup_file.hpp>
#include <dual/nds/arm7/spi.hpp>

namespace dual::nds {

  // EEPROM 512B memory emulation
  class EEPROM512B final : public arm7::SPI::BackupDevice {
    public:
      explicit EEPROM512B(const std::string& save_path);

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
        Read         = 0x03, // RDLO & RDHI
        Write        = 0x02  // WRLO & WRHI
      };

      enum class State {
        Idle,
        Deselected,
        ReceiveCommand,
        ReadStatus,
        WriteStatus,
        ReadAddress,
        Read,
        Write
      } m_state{};

      void ParseCommand(u8 cmd);

      Command m_current_cmd{};
      u16 m_address{};
      bool m_write_enable_latch{};
      int  m_write_protect_mode{};

      std::string m_save_path{};

      std::unique_ptr<BackupFile> m_file{};
  };

} // namespace dual::nds

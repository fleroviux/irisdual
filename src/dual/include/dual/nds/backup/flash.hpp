
#pragma once

#include <dual/common/backup_file.hpp>
#include <dual/nds/arm7/spi.hpp>

namespace dual::nds {

  // FLASH memory emulation
  class FLASH final : public arm7::SPI::BackupDevice {
    public:
      enum class Size {
        _256K = 0,
        _512K,
        _1024K,
        _8192K
      };

      FLASH(const std::string& save_path, Size size_hint);

      void Reset() override;
      void Select() override;
      void Deselect() override;

      u8 Transfer(u8 data) override;

    private:
      enum class Command : u8 {
        WriteEnable   = 0x06, // WREM
        WriteDisable  = 0x04, // WRDI
        ReadJEDEC     = 0x96, // RDID
        ReadStatus    = 0x05, // RDSR
        ReadData      = 0x03, // READ
        ReadDataFast  = 0x0B, // FAST
        PageWrite     = 0x0A, // PW
        PageProgram   = 0x02, // PP
        PageErase     = 0xDB, // PE
        SectorErase   = 0xD8, // SE
        DeepPowerDown = 0xB9, // DP
        ReleaseDeepPowerDown = 0xAB // RDP
      };

      enum class State {
        Idle,
        Deselected,
        ReceiveCommand,
        ReadJEDEC,
        ReadStatus,
        SendAddress0,
        SendAddress1,
        SendAddress2,
        DummyByte,
        ReadData,
        PageWrite,
        PageProgram,
        PageErase,
        SectorErase
      } m_state{};

      void ParseCommand(Command cmd);

      Command m_current_cmd{};
      u32 m_address{};
      bool m_write_enable_latch{};
      bool m_deep_power_down{};
      u8 m_jedec_id[3] { 0x20, 0x40, 0x12 };

      std::string m_save_path{};
      Size m_size_hint{};
      size_t m_mask{};

      std::unique_ptr<BackupFile> m_file;
  };

} // namespace dual::nds

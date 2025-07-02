
#pragma once

#include <atom/literal.hpp>
#include <dual/nds/arm7/spi.hpp>
#include <dual/nds/backup/eeprom512b.hpp>
#include <dual/nds/backup/eeprom.hpp>
#include <dual/nds/backup/flash.hpp>
#include <memory>

namespace dual::nds {

  class AutomaticBackupDevice final : public arm7::SPI::BackupDevice {
    public:
      explicit AutomaticBackupDevice(const std::string& save_path);

      void SetDeviceHint(Type type, size_t byte_size) override;
      void Reset() override;
      void Select() override;
      void Deselect() override;
      u8 Transfer(u8 data) override;

    private:
      std::unique_ptr<arm7::SPI::BackupDevice> CreateBackupDevice(Type type, size_t byte_size);

      std::string m_save_path{};
      std::unique_ptr<arm7::SPI::BackupDevice> m_actual_backup{};
  };

} // namespace dual::nds

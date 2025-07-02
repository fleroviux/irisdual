

#include <atom/panic.hpp>
#include <dual/nds/backup/automatic.hpp>

namespace dual::nds {

  AutomaticBackupDevice::AutomaticBackupDevice(const std::string& save_path)
      : m_save_path(save_path) {
  }

  void AutomaticBackupDevice::SetDeviceHint(Type type, size_t byte_size) {
    m_actual_backup = CreateBackupDevice(type, byte_size);
  }

  void AutomaticBackupDevice::Reset() {
    if(m_actual_backup) {
      m_actual_backup->Reset();
    }
  }

  void AutomaticBackupDevice::Select() {
    if(m_actual_backup) {
      m_actual_backup->Select();
    }
  }

  void AutomaticBackupDevice::Deselect() {
    if(m_actual_backup) {
      m_actual_backup->Deselect();
    }
  }

  u8 AutomaticBackupDevice::Transfer(u8 data) {
    if(m_actual_backup) {
      return m_actual_backup->Transfer(data);
    }
    return 0xffu;
  }

  std::unique_ptr<arm7::SPI::BackupDevice> AutomaticBackupDevice::CreateBackupDevice(Type type, size_t byte_size) {
    using namespace atom::literals;

    switch(type) {
      case Type::None: {
        break;
      }
      case Type::EEPROM: {
        switch(byte_size) {
          case     512: return std::make_unique<EEPROM512B>(m_save_path);
          case   8_KiB: return std::make_unique<EEPROM>(m_save_path,   dual::nds::EEPROM::Size::_8K, false);
          case  64_KiB: return std::make_unique<EEPROM>(m_save_path,  dual::nds::EEPROM::Size::_64K, false);
          case 128_KiB: return std::make_unique<EEPROM>(m_save_path, dual::nds::EEPROM::Size::_128K, false);
        }
        break;
      }
      case Type::FLASH: {
        switch(byte_size) {
          case  256_KiB: return std::make_unique<FLASH>(m_save_path,  dual::nds::FLASH::Size::_256K);
          case  512_KiB: return std::make_unique<FLASH>(m_save_path,  dual::nds::FLASH::Size::_512K);
          case 1024_KiB: return std::make_unique<FLASH>(m_save_path, dual::nds::FLASH::Size::_1024K);
          case 8192_KiB: return std::make_unique<FLASH>(m_save_path, dual::nds::FLASH::Size::_8192K);
        }
        break;
      }
      case Type::FRAM: {
        switch(byte_size) {
          case  8_KiB: return std::make_unique<EEPROM>(m_save_path,   dual::nds::EEPROM::Size::_8K, true);
          case 32_KiB: return std::make_unique<EEPROM>(m_save_path,  dual::nds::EEPROM::Size::_32K, true);
        }
        break;
      }
    }

    return {};
  }

} // namespace dual::nds

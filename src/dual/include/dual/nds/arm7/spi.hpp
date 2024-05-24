
#pragma once

#include <atom/bit.hpp>
#include <atom/integer.hpp>
#include <dual/nds/irq.hpp>
#include <memory>

namespace dual::nds {
  class FLASH;
} // namespace dual::nds

namespace dual::nds::arm7 {

  class TouchScreen;

  class SPI {
    public:
      struct Device {
        virtual ~Device() = default;

        virtual void Reset() = 0;
        virtual void Select() = 0;
        virtual void Deselect() = 0;

        virtual u8 Transfer(u8 data) = 0;
      };

      explicit SPI(IRQ& irq);

      void Reset();

      auto  Read_SPICNT() -> u16;
      void Write_SPICNT(u16 value, u16 mask);

      auto  Read_SPIDATA() -> u8;
      void Write_SPIDATA(u8 value);

      [[nodiscard]] TouchScreen& GetTouchScreen() {
        return (TouchScreen&)*m_touch_screen;
      }

    private:
      void ReadAndApplyTouchScreenCalibrationData();

      union SPICNT {
        atom::Bits< 0, 2, u16> baud_rate;
        atom::Bits< 7, 1, u16> busy;
        atom::Bits< 8, 2, u16> device;
        atom::Bits<10, 1, u16> enable_16bit_mode;
        atom::Bits<11, 1, u16> chip_select_hold;
        atom::Bits<14, 1, u16> enable_irq;
        atom::Bits<15, 1, u16> enable;

        u16 half = 0u;
      } m_spicnt{};

      u8 m_spidata{};

      bool m_chip_select{};

      IRQ& m_irq;

      std::unique_ptr<Device> m_firmware{};
      std::unique_ptr<Device> m_touch_screen{};
      Device* m_device_table[4]{};
  };

} // namespace dual::nds::arm7
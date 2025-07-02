
#pragma once

#include <atom/bit.hpp>
#include <dual/arm/memory.hpp>
#include <dual/common/fifo.hpp>
#include <dual/nds/cartridge.hpp>
#include <dual/nds/enums.hpp>
#include <dual/nds/irq.hpp>

namespace dual::nds {

  // Inter-Process Communication hardware for ARM9 and ARM7 synchronization and message passing.
  class IPC {
    public:
      IPC(IRQ& irq9, IRQ& irq7, dual::arm::Memory& bus7, Cartridge& cartridge);

      void Reset();

      u32   Read_SYNC(CPU cpu);
      void Write_SYNC(CPU cpu, u32 value, u32 mask);

      u32   Read_FIFOCNT(CPU cpu);
      void Write_FIFOCNT(CPU cpu, u32 value, u32 mask);

      u32   Read_FIFORECV(CPU cpu);
      void Write_FIFOSEND(CPU cpu, u32 value);

    private:
      union IPCSYNC {
        atom::Bits< 0, 4, u32> recv;
        atom::Bits< 8, 4, u32> send;
        atom::Bits<14, 1, u32> enable_remote_irq;

        u32 word = 0u;
      } m_sync[2];

      struct IPCFIFO {
        FIFO<u32, 16> send{};

        u32 latch = 0u;

        union {
          atom::Bits< 2, 1, u32> enable_send_fifo_irq;
          atom::Bits<10, 1, u32> enable_recv_fifo_irq;
          atom::Bits<14, 1, u32> error_flag;
          atom::Bits<15, 1, u32> enable;

          u32 word = 0u;
        } control{};
      } m_fifo[2];

      IRQ* m_irq[2]{};
      dual::arm::Memory& m_bus7;
      Cartridge& m_cartridge;

      bool m_got_first_fs_command{};
  };

} // namespace dual::nds

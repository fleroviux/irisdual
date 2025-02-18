
#include <algorithm>
#include <atom/punning.hpp>
#include <dual/nds/arm7/touch_screen.hpp>
#include <dual/nds/backup/flash.hpp>
#include <dual/nds/nds.hpp>
#include <dual/nds/header.hpp>
#include <dual/nds/backup/eeprom.hpp>

#include "arm/dynarec/dynarec_cpu.hpp"
#include "arm/interpreter/interpreter_cpu.hpp"
#ifdef DUAL_ENABLE_JIT
  #include "arm/jit/lunatic_cpu.hpp"
#endif

namespace dual::nds {

  NDS::NDS() {
    m_arm9.cp15 = std::make_unique<arm9::CP15>(&m_arm9.bus);
  }

  void NDS::SetCPUExecutionEngine(CPUExecutionEngine cpu_execution_engine) {
    m_cpu_execution_engine = cpu_execution_engine;
  }

  void NDS::Reset() {
    CreateCPUCores();

    m_scheduler.Reset();

    m_video_unit.Reset();

    m_cartridge.Reset();

    m_key_input = 0x007F03FFu;

    m_memory.ewram.fill(0);
    m_memory.swram.Reset();
    m_memory.vram.Reset();
    m_memory.pram.fill(0);
    m_memory.oam.fill(0);
    m_memory.arm9.dtcm.fill(0);
    m_memory.arm9.itcm.fill(0);
    m_memory.arm7.iwram.fill(0);

    m_arm9.cp15->Reset();
    m_arm9.cycle_counter.Reset();
    m_arm7.cycle_counter.Reset();
    m_arm9.cpu->Reset();
    m_arm7.cpu->Reset();

    m_arm9.bus.Reset();
    m_arm7.bus.Reset();

    m_arm9.irq.Reset();
    m_arm7.irq.Reset();

    m_arm9.timer.Reset();
    m_arm7.timer.Reset();

    m_arm9.dma.Reset();
    m_arm7.dma.Reset();

    m_arm7.spi.Reset();
    m_arm7.rtc.Reset();
    m_arm7.apu.Reset();
    m_arm7.wifi.Reset();

    m_ipc.Reset();

    m_step_target = 0u;
  }

  void NDS::CreateCPUCores() {
    const arm::AttachCPn attach_cp15{.id = 15, .coprocessor = m_arm9.cp15.get()};

    switch(m_cpu_execution_engine) {
      case CPUExecutionEngine::Interpreter: {
        m_arm9.cpu = std::make_unique<arm::InterpreterCPU>(m_arm9.bus, m_scheduler, m_arm9.cycle_counter, arm::CPU::Model::ARM9, std::span<const arm::AttachCPn>{{attach_cp15}});
        m_arm7.cpu = std::make_unique<arm::InterpreterCPU>(m_arm7.bus, m_scheduler, m_arm7.cycle_counter, arm::CPU::Model::ARM7);
        break;
      }
#ifdef DUAL_ENABLE_JIT
      case CPUExecutionEngine::JIT: {
        m_arm9.cpu = std::make_unique<arm::LunaticCPU>(m_arm9.bus, m_arm9.cycle_counter, arm::CPU::Model::ARM9, std::span<const arm::AttachCPn>{{attach_cp15}});
        m_arm7.cpu = std::make_unique<arm::LunaticCPU>(m_arm7.bus, m_arm7.cycle_counter, arm::CPU::Model::ARM7);
        break;
      }
#endif
      default: ATOM_PANIC("unknown CPU emulator");
    }

    // @todo: integrate the new dynamic recompiler more cleanly
    m_arm7.cpu = std::make_unique<arm::DynarecCPU>(m_arm7.bus, m_scheduler, m_arm7.cycle_counter, arm::CPU::Model::ARM7);

    m_arm9.irq.SetCPU(m_arm9.cpu.get());
    m_arm7.irq.SetCPU(m_arm7.cpu.get());
  }

  void NDS::Step(int cycles_to_run) {
    const u64 step_target = m_step_target + cycles_to_run;

    while(m_scheduler.GetTimestampNow() < step_target) {
      const u64 target = std::min(m_scheduler.GetTimestampTarget(), step_target);

      // Run both CPUs for up to 32 cycles. Skip to the next event if both CPUs are halting.
      int cycles = static_cast<int>(target - m_scheduler.GetTimestampNow());
      if(!m_arm9.cpu->GetWaitingForIRQ() || !m_arm7.cpu->GetWaitingForIRQ()) {
        cycles = std::min(cycles, 32);
      }

      m_arm9.cpu->Run(cycles * 2);
      m_arm7.cpu->Run(cycles);

      m_scheduler.AddCycles(cycles);
    }

    m_step_target = step_target;
  }

  void NDS::LoadBootROM9(std::span<u8, 0x8000> data) {
    std::copy(data.begin(), data.end(), m_memory.arm9.bios.begin());
  }

  void NDS::LoadBootROM7(std::span<u8, 0x4000> data) {
    std::copy(data.begin(), data.end(), m_memory.arm7.bios.begin());
  }

  void NDS::LoadROM(std::shared_ptr<ROM> rom, std::shared_ptr<dual::nds::arm7::SPI::Device> backup) {
    m_cartridge.SetROM(rom, backup);
    m_rom = std::move(rom);
  }

  void NDS::DirectBoot() {
    Header header{};

    if(m_rom->Size() < sizeof(Header)) {
      ATOM_PANIC("the loaded ROM is too small");
    }

    Reset();

    m_rom->Read(reinterpret_cast<u8*>(&header), 0, sizeof(Header));
    m_rom->Read(&m_memory.ewram[0x3FFE00], 0, 0x170);

    const auto LoadBinary = [&](Header::Binary& binary, bool arm9) {
      const char* name = arm9 ? "ARM9" : "ARM7";

      const u32 size = binary.size;

      const u32 file_address_lo = binary.file_address;
      const u32 file_address_hi = file_address_lo + size;

      const u32 load_address_lo = binary.load_address;
      const u32 load_address_hi = load_address_lo + size;

      bool bad_header;

      bad_header  = file_address_hi > m_rom->Size();
      bad_header |= file_address_hi < file_address_lo;
      bad_header |= load_address_hi < load_address_lo;
      bad_header |= (size & 3u) != 0u;

      if(bad_header) {
        ATOM_PANIC("bad NDS file header (bad {} binary descriptor)", name);
      }

      if(arm9) {
        for(u32 i = 0; i < size; i += 4u) {
          u32 word;

          m_rom->Read((u8*)&word, file_address_lo + i, sizeof(u32));
          m_arm9.bus.WriteWord(load_address_lo + i, word, dual::arm::Memory::Bus::System);
        }
      } else {
        for(u32 i = 0; i < size; i += 4u) {
          u32 word;

          m_rom->Read((u8*)&word, file_address_lo + i, sizeof(u32));
          m_arm7.bus.WriteWord(load_address_lo + i, word, dual::arm::Memory::Bus::System);
        }
      }
    };

    LoadBinary(header.arm9, true);
    LoadBinary(header.arm7, false);

    using Mode = arm::CPU::Mode;

    m_arm9.cpu->SetGPR(arm::CPU::GPR::SP, Mode::System,     0x03002F7Cu);
    m_arm9.cpu->SetGPR(arm::CPU::GPR::SP, Mode::IRQ,        0x03003F80u);
    m_arm9.cpu->SetGPR(arm::CPU::GPR::SP, Mode::Supervisor, 0x03003FC0u);
    m_arm9.cpu->SetGPR(arm::CPU::GPR::PC, header.arm9.entrypoint + 8u);
    m_arm9.cpu->SetCPSR(static_cast<u32>(Mode::System));

    m_arm9.cp15->DirectBoot();

    m_arm7.cpu->SetGPR(arm::CPU::GPR::SP, Mode::System,     0x0380FD80u);
    m_arm7.cpu->SetGPR(arm::CPU::GPR::SP, Mode::IRQ,        0x0380FF80u);
    m_arm7.cpu->SetGPR(arm::CPU::GPR::SP, Mode::Supervisor, 0x0380FFC0u);
    m_arm7.cpu->SetGPR(arm::CPU::GPR::PC, header.arm7.entrypoint + 8u);
    m_arm7.cpu->SetCPSR(static_cast<u32>(Mode::System));

    /**
     * This is required for direct booting commercial ROMs.
     * Thank you Hydr8gon for pointing it out to me.
     */
    m_arm9.bus.WriteWord(0x027FF800u, 0x1FC2u, dual::arm::Memory::Bus::Data); // Chip ID 1
    m_arm9.bus.WriteWord(0x027FF804u, 0x1FC2u, dual::arm::Memory::Bus::Data); // Chip ID 2
    m_arm9.bus.WriteHalf(0x027FF850u, 0x5835u, dual::arm::Memory::Bus::Data); // ARM7 BIOS CRC
    m_arm9.bus.WriteHalf(0x027FF880u, 0x0007u, dual::arm::Memory::Bus::Data); // Message from ARM9 to ARM7
    m_arm9.bus.WriteHalf(0x027FF884u, 0x0006u, dual::arm::Memory::Bus::Data); // ARM7 boot task
    m_arm9.bus.WriteWord(0x027FFC00u, 0x1FC2u, dual::arm::Memory::Bus::Data); // Copy of chip ID 1
    m_arm9.bus.WriteWord(0x027FFC04u, 0x1FC2u, dual::arm::Memory::Bus::Data); // Copy of chip ID 2
    m_arm9.bus.WriteHalf(0x027FFC10u, 0x5835u, dual::arm::Memory::Bus::Data); // Copy of ARM7 BIOS CRC
    m_arm9.bus.WriteHalf(0x027FFC40u, 0x0001u, dual::arm::Memory::Bus::Data); // Boot indicator

    {
      FLASH& firmware = m_arm7.spi.GetFirmwareFlash();

      firmware.Select();

      // Command: READ_DATA
      firmware.Transfer(0x03u);

      // Address: 0x3FE00u
      firmware.Transfer(0x03u);
      firmware.Transfer(0xFEu);
      firmware.Transfer(0x00u);

      for(u32 address = 0x027FFC80; address < 0x027FFCF0; address++) {
        m_arm9.bus.WriteByte(address, firmware.Transfer(0u), dual::arm::Memory::Bus::Data);
      }

      firmware.Deselect();
    }

    m_arm9.bus.WriteByte(0x04000300, 1u, dual::arm::Memory::Bus::Data);
    m_arm7.bus.WriteByte(0x04000300, 1u, dual::arm::Memory::Bus::Data);

    m_cartridge.DirectBoot();

    m_video_unit.DirectBoot();
  }

  void NDS::SetKeyState(Key key, bool pressed) {
    if(pressed) {
      m_key_input &= ~(1u << (int)key);
    } else {
      m_key_input |=   1u << (int)key;
    }
  }

  void NDS::SetTouchState(bool pen_down, u8 x, u8 y) {
    m_arm7.spi.GetTouchScreen().SetTouchState(pen_down, x, y);

    if(pen_down) {
      m_key_input &= ~(1 << 22);
    } else {
      m_key_input |=   1 << 22;
    }
  }

} // namespace dual::nds

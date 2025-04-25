
#include <atom/logger/logger.hpp>
#include <atom/panic.hpp>
#include <dual/nds/arm9/cp15.hpp>

#define ID(opc1, cn, cm, opc2) ((opc1) << 11 | (cn) << 7 | (cm) << 3 | (opc2))

namespace dual::nds::arm9 {

  CP15::CP15(MemoryBus* bus) : m_bus{bus} {
  }

  void CP15::Reset() {
    // @todo: refer to ARM9 manual to figure out correct initialization values
    DirectBoot();
  }

  void CP15::SetCPU(arm::CPU* cpu) {
    m_cpu = cpu;
  }

  void CP15::DirectBoot() {
    // Reset control register (enable DTCM and ITCM, exception base = 0xFFFF0000)
    MCR(0, 1, 0, 0, 0x0005707Du);

    // Reset DTCM and ITCM configuration
    MCR(0, 9, 1, 0, 0x0300000Au);
    MCR(0, 9, 1, 1, 0x00000020u);

    // Reset Process ID register
    MCR(0, 13, 0, 1, 0x00000000u);
  }

  u32 CP15::MRC(int opc1, int cn, int cm, int opc2) {
    switch(ID(opc1, cn, cm, opc2)) {
      case ID(0, 0, 0, 0): { // Main ID
        return 0x41059461u;
      }
      case ID(0, 0, 0, 1): { // Cache type
        return 0x0F0D2112u;
      }
      case ID(0, 1, 0, 0): { // Control register
        return m_control.word;
      }
      case ID(0, 9, 1, 0): { // DTCM region register
        return m_dtcm_region;
      }
      case ID(0, 9, 1, 1): { // ITCM region register
        return m_itcm_region;
      }
      case ID(0, 13, 0, 1):
      case ID(0, 13, 1, 1): { // Trace Process ID
        return m_trace_process_id;
      }
      default: {
        ATOM_WARN("arm9: CP15: unhandled MRC #{}, C{}, C{}, #{}", opc1, cn, cm, opc2);
      }
    }

    return 0u;
  }

  void CP15::MCR(int opc1, int cn, int cm, int opc2, u32 value) {
    switch(ID(opc1, cn, cm, opc2)) {
      case ID(0, 1, 0, 0): { // Control register
        m_control.word = (value & 0x000FF085u) | 0x78u;

        m_cpu->SetExceptionBase(m_control.alternate_vector_select ? 0xFFFF0000u : 0u);

        m_dtcm_config.writable = m_control.enable_dtcm;
        m_dtcm_config.readable = m_control.enable_dtcm && m_control.dtcm_load_mode == 0u;
        m_bus->SetupDTCM(m_dtcm_config);

        m_itcm_config.writable = m_control.enable_itcm;
        m_itcm_config.readable = m_control.enable_itcm && m_control.itcm_load_mode == 0u;
        m_bus->SetupITCM(m_itcm_config);

        if(m_control.enable_big_endian_mode) {
          ATOM_PANIC("arm9: CP15: enabled unemulated big-endian mode");
        }

        if(m_control.disable_loading_tbit) {
          ATOM_PANIC("arm9: CP15: enabled unemulated ARMv4T compatibility mode");
        }
        break;
      }
      case ID(0, 7, 0, 4): { // Wait for IRQ
        m_cpu->SetWaitingForIRQ(true);
        break;
      }
      case ID(0, 7, 5, 0): { // Invalidate ICache
        m_cpu->InvalidateICache();
        break;
      }
      case ID(0, 7, 5, 1): { // Invalidate ICache Line
        const u32 address_lo = value & ~0x1Fu;
        const u32 address_hi = address_lo + 0x1Fu;
        m_cpu->InvalidateICacheRange(address_lo, address_hi);
        break;
      }
      case ID(0, 9, 1, 0): { // DTCM region register
        const int size = static_cast<int>((value >> 1) & 0x1Fu);

        if(size < 3u || size > 23u) {
          ATOM_PANIC("arm9: CP15: DTCM virtual size was not between 4 KiB (3) and 4 GiB (23)");
        }

        const u32 base_address = value & ~0xFFFu;
        const u32 high_address = base_address + (512 << size) - 1;

        if(high_address < base_address) {
          ATOM_PANIC("arm9: CP15: DTCM high address is below base address")
        }

        m_dtcm_config.base_address = base_address;
        m_dtcm_config.high_address = high_address;

        m_bus->SetupDTCM(m_dtcm_config);

        m_dtcm_region = value & ~1u;

        ATOM_DEBUG("arm9: CP15: DTCM mapped @ 0x{:08X} - 0x{:08X}", base_address, high_address);
        break;
      }
      case ID(0, 9, 1, 1): { // ITCM region register
        const int size = static_cast<int>((value >> 1) & 0x1Fu);

        if(size < 3u || size > 23u) {
          ATOM_PANIC("arm9: CP15: ITCM virtual size was not between 4 KiB (3) and 4 GiB (23)");
        }

        const u32 base_address = value & ~0xFFFu;
        const u32 high_address = base_address + (512 << size) - 1;

        if(high_address < base_address) {
          ATOM_PANIC("arm9: CP15: ITCM high address is below base address")
        }

        m_itcm_config.base_address = base_address;
        m_itcm_config.high_address = high_address;

        m_bus->SetupITCM(m_itcm_config);

        m_itcm_region = value & ~1u;

        ATOM_DEBUG("arm9: CP15: ITCM mapped @ 0x{:08X} - 0x{:08X}", base_address, high_address);
        break;
      }
      case ID(0, 13, 0, 1):
      case ID(0, 13, 1, 1): { // Trace Process ID
        m_trace_process_id = value;
        break;
      }
      default: {
        ATOM_WARN("arm9: CP15: unhandled MCR #{}, C{}, C{}, #{} = 0x{:08X}", opc1, cn, cm, opc2, value);
      }
    }
  }

} // namespace dual::nds::arm9

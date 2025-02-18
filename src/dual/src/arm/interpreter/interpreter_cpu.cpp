
#include "interpreter_cpu.hpp"

namespace dual::arm {

  InterpreterCPU::InterpreterCPU(
    Memory& memory,
    Scheduler& scheduler,
    CycleCounter& cycle_counter,
    Model model,
    std::span<const AttachCPn> coprocessor_table
  )   : m_memory{memory}
      , m_scheduler{scheduler}
      , m_cycle_counter{cycle_counter}
      , m_model{model} {
    m_unaligned_data_access_enable = false;

    BuildConditionTable();
    Reset();

    for(auto& attach_cp_n : coprocessor_table) {
      m_coprocessors.at(attach_cp_n.id) = attach_cp_n.coprocessor;
      attach_cp_n.coprocessor->SetCPU(this);
    }
  }

  void InterpreterCPU::Reset() {
    constexpr u32 nop = 0xE320F000;

    m_state = {};
    SwitchMode((Mode)m_state.cpsr.mode);
    m_opcode[0] = nop;
    m_opcode[1] = nop;
    m_state.r15 = m_exception_base;
    m_wait_for_irq = false;
    SetIRQFlag(false);
  }

  void InterpreterCPU::Run(int cycles) {
    if(GetWaitingForIRQ()) {
      m_cycle_counter.AddDeviceCycles((uint)cycles);
      return;
    }

    while(cycles-- > 0 && m_cycle_counter.GetTimestampNow() < m_scheduler.GetTimestampTarget()) {
      if(GetIRQFlag()) {
        SignalIRQ();
      }

      const u32 instruction = m_opcode[0];

      if(m_state.cpsr.thumb) {
        m_state.r15 &= ~1;

        m_opcode[0] = m_opcode[1];
        m_opcode[1] = ReadHalfCode(m_state.r15);

        (this->*k_opcode_lut_16[instruction >> 5])(instruction);
      } else {
        m_state.r15 &= ~3;

        m_opcode[0] = m_opcode[1];
        m_opcode[1] = ReadWordCode(m_state.r15);

        const auto condition = static_cast<Condition>(instruction >> 28);

        if(EvaluateCondition(condition)) {
          int hash = static_cast<int>(((instruction >> 16) & 0xFF0) | ((instruction >> 4) & 0x00F));

          if(condition == Condition::NV) {
            hash |= 4096;
          }

          (this->*k_opcode_lut_32[hash])(instruction);
        } else {
          m_state.r15 += 4;
        }
      }

      m_cycle_counter.AddDeviceCycles(1u);

      if(GetWaitingForIRQ()) {
        m_cycle_counter.AddDeviceCycles(cycles);
        return;
      }
    }
  }

  void InterpreterCPU::SignalIRQ() {
    if(m_state.cpsr.mask_irq) {
      return;
    }

    // Save current program status register.
    m_state.spsr[(int)Bank::IRQ] = m_state.cpsr;

    // Enter IRQ mode and disable IRQs.
    SwitchMode(Mode::IRQ);
    m_state.cpsr.mask_irq = 1;

    // Save current program counter and disable Thumb.
    if(m_state.cpsr.thumb) {
      m_state.cpsr.thumb = 0;
      m_state.r14 = m_state.r15;
    } else {
      m_state.r14 = m_state.r15 - 4;
    }

    // Jump to IRQ exception vector.
    m_state.r15 = m_exception_base + 0x18;
    ReloadPipeline32();
  }

  void InterpreterCPU::ReloadPipeline32() {
    m_opcode[0] = ReadWordCode(m_state.r15);
    m_opcode[1] = ReadWordCode(m_state.r15 + 4u);
    m_state.r15 += 8u;
  }

  void InterpreterCPU::ReloadPipeline16() {
    m_opcode[0] = ReadHalfCode(m_state.r15);
    m_opcode[1] = ReadHalfCode(m_state.r15 + 2u);
    m_state.r15 += 4u;
  }

  void InterpreterCPU::ReloadPipelineOnSetR15OrCPSR() {
    if(m_state.cpsr.thumb) {
      m_opcode[0] = ReadHalfCode(m_state.r15 - 4u);
      m_opcode[1] = ReadHalfCode(m_state.r15 - 2u);
    } else {
      m_opcode[0] = ReadWordCode(m_state.r15 - 8u);
      m_opcode[1] = ReadWordCode(m_state.r15 - 4u);
    }
  }

  void InterpreterCPU::BuildConditionTable() {
    for(int flags = 0; flags < 16; flags++) {
      bool n = flags & 8;
      bool z = flags & 4;
      bool c = flags & 2;
      bool v = flags & 1;

      m_condition_table[(int)Condition::EQ][flags] = z;
      m_condition_table[(int)Condition::NE][flags] = !z;
      m_condition_table[(int)Condition::CS][flags] =  c;
      m_condition_table[(int)Condition::CC][flags] = !c;
      m_condition_table[(int)Condition::MI][flags] =  n;
      m_condition_table[(int)Condition::PL][flags] = !n;
      m_condition_table[(int)Condition::VS][flags] =  v;
      m_condition_table[(int)Condition::VC][flags] = !v;
      m_condition_table[(int)Condition::HI][flags] =  c && !z;
      m_condition_table[(int)Condition::LS][flags] = !c ||  z;
      m_condition_table[(int)Condition::GE][flags] = n == v;
      m_condition_table[(int)Condition::LT][flags] = n != v;
      m_condition_table[(int)Condition::GT][flags] = !(z || (n != v));
      m_condition_table[(int)Condition::LE][flags] =  (z || (n != v));
      m_condition_table[(int)Condition::AL][flags] = true;
      m_condition_table[(int)Condition::NV][flags] = true;
    }
  }

  auto InterpreterCPU::GetRegisterBankByMode(Mode mode) -> Bank {
    switch(mode) {
      case Mode::User:       return Bank::None;
      case Mode::System:     return Bank::None;
      case Mode::FIQ:        return Bank::FIQ;
      case Mode::IRQ:        return Bank::IRQ;
      case Mode::Supervisor: return Bank::Supervisor;
      case Mode::Abort:      return Bank::Abort;
      case Mode::Undefined:  return Bank::Undefined;
    }

    ATOM_PANIC("invalid ARM CPU mode: 0x{:02X}", (uint)mode);
  }

  void InterpreterCPU::SwitchMode(Mode new_mode) {
    auto old_bank = GetRegisterBankByMode((Mode)m_state.cpsr.mode);
    auto new_bank = GetRegisterBankByMode(new_mode);

    m_state.cpsr.mode = new_mode;
    m_spsr = &m_state.spsr[(int)new_bank];

    if(old_bank == new_bank) {
      return;
    }

    if(old_bank == Bank::FIQ) {
      for(int i = 0; i < 5; i++){
        m_state.bank[(int)Bank::FIQ][i] = m_state.reg[8 + i];
      }

      for(int i = 0; i < 5; i++) {
        m_state.reg[8 + i] = m_state.bank[(int)Bank::None][i];
      }
    } else if(new_bank == Bank::FIQ) {
      for(int i = 0; i < 5; i++) {
        m_state.bank[(int)Bank::None][i] = m_state.reg[8 + i];
      }

      for(int i = 0; i < 5; i++) {
        m_state.reg[8 + i] = m_state.bank[(int)Bank::FIQ][i];
      }
    }

    m_state.bank[(int)old_bank][5] = m_state.r13;
    m_state.bank[(int)old_bank][6] = m_state.r14;

    m_state.r13 = m_state.bank[(int)new_bank][5];
    m_state.r14 = m_state.bank[(int)new_bank][6];
  }

} // namespace dual::arm

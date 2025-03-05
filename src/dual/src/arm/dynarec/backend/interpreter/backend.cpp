
#include <atom/panic.hpp>
#include <optional>

#include "arm/dynarec/ir/instruction.hpp"
#include "backend.hpp"

namespace dual::arm::jit {

static bool EvaluateCondition(ir::Condition condition, u32 flags);

InterpreterBackend::InterpreterBackend(State& cpu_state)
    : m_cpu_state{cpu_state} {
}

void InterpreterBackend::Execute(const ir::Function& function, bool debug) {
  std::optional<ir::BasicBlock::ID> current_bb = 0u;

  while(current_bb.has_value()) {
    const ir::BasicBlock& basic_block = *function.basic_blocks[current_bb.value()];

    // Allocate one host register for each IR value
    m_host_regs.resize(basic_block.values.size());

    ir::Instruction* instruction = basic_block.head;

    while(instruction != nullptr) {
      using InstructionType = ir::Instruction::Type;

      switch(instruction->type) {
        // Constant Loading
        case InstructionType::LDCONST: {
          const u32 const_u32 = instruction->GetArg(0u).AsConstU32();
          const ir::Value::ID result_value = instruction->GetOut(0u);
          m_host_regs[result_value].data_u32 = const_u32;
          break;
        }

        // Guest State Read/Write
        case InstructionType::LDGPR: {
          const ir::GPR gpr = instruction->GetArg(0u).AsGPR();
          const ir::Mode cpu_mode = instruction->GetArg(1u).AsMode();
          const ir::Value::ID result_value = instruction->GetOut(0u);
          m_host_regs[result_value].data_u32 = m_cpu_state.GetGPR(gpr, cpu_mode);
          break;
        }
        case InstructionType::STGPR: {
          const ir::GPR gpr = instruction->GetArg(0u).AsGPR();
          const ir::Mode cpu_mode = instruction->GetArg(1u).AsMode();
          const ir::Value::ID source_value = instruction->GetArg(2u).AsValue();
          m_cpu_state.SetGPR(gpr, cpu_mode, m_host_regs[source_value].data_u32);
          break;
        }
        case InstructionType::LDCPSR: {
          const ir::Value::ID result_value = instruction->GetOut(0u);
          m_host_regs[result_value].data_u32 = m_cpu_state.GetCPSR().word;
          break;
        }
        case InstructionType::STCPSR: {
          const u32 source = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          m_cpu_state.SetCPSR(State::PSR{source});
          break;
        }
        case InstructionType::LDSPSR: {
          const ir::Mode cpu_mode = instruction->GetArg(0u).AsMode();
          const ir::Value::ID result_value = instruction->GetOut(0u);
          m_host_regs[result_value].data_u32 = m_cpu_state.GetSPSR(cpu_mode).word;
          break;
        }
        case InstructionType::STSPSR: {
          const ir::Mode cpu_mode = instruction->GetArg(0u).AsMode();
          const u32 source = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          m_cpu_state.SetSPSR(cpu_mode, State::PSR{source});
          break;
        }

        // Flag Management
        case InstructionType::CVT_HFLAG_NZCV:
        case InstructionType::CVT_NZCV_HFLAG: {
          const ir::Value::ID source_value = instruction->GetArg(0u).AsValue();
          const ir::Value::ID result_value = instruction->GetOut(0u);
          m_host_regs[result_value].data_u32 = m_host_regs[source_value].data_u32;
          break;
        }

        // Control Flow
        case InstructionType::BR_IF: {
          const ir::Condition condition = instruction->GetArg(0u).AsCondition();
          const u32 hflag = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          if(EvaluateCondition(condition, hflag)) {
            current_bb = instruction->GetArg(2u).AsBasicBlock();
          } else {
            current_bb = instruction->GetArg(3u).AsBasicBlock();
          }
          break;
        }
        case InstructionType::EXIT: {
          current_bb = std::nullopt;
          break;
        }

        // Barrel Shifter
        case InstructionType::LSL: {
          const u32 value = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          const u32 shift_amount = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          if(shift_amount >= 32) {
            ATOM_PANIC("LSL #{} is UB", shift_amount);
          }
          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            const ir::Value::ID hflag_value = instruction->GetOut(1u);
            if(shift_amount == 0) {
              m_host_regs[hflag_value].data_u32 = 0u;
            } else {
              m_host_regs[hflag_value].data_u32 = value << (shift_amount - 1) >> 2;
            }
          }
          m_host_regs[instruction->GetOut(0u)].data_u32 = value << shift_amount;
          break;
        }
        case InstructionType::LSR: {
          const u32 value = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          const u32 shift_amount = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          if(shift_amount >= 32) {
            ATOM_PANIC("LSR #{} is UB", shift_amount);
          }
          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            const ir::Value::ID hflag_value = instruction->GetOut(1u);
            if(shift_amount == 0) {
              m_host_regs[hflag_value].data_u32 = 0u;
            } else {
              m_host_regs[hflag_value].data_u32 = value >> (shift_amount - 1) << 29;
            }
          }
          m_host_regs[instruction->GetOut(0u)].data_u32 = value >> shift_amount;
          break;
        }
        case InstructionType::ASR: {
          const u32 value = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          const u32 shift_amount = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          if(shift_amount >= 32) {
            ATOM_PANIC("ASR #{} is UB", shift_amount);
          }
          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            const ir::Value::ID hflag_value = instruction->GetOut(1u);
            if(shift_amount == 0) {
              m_host_regs[hflag_value].data_u32 = 0u;
            } else {
              m_host_regs[hflag_value].data_u32 = value >> (shift_amount - 1) << 29;
            }
          }
          m_host_regs[instruction->GetOut(0u)].data_u32 = (u32)((i32)value >> shift_amount);
          break;
        }

        // Data Processing
        case InstructionType::ADD: {
          const u32 lhs = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          const u32 rhs = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          const u32 result = lhs + rhs;

          const ir::Value::ID result_value = instruction->GetOut(0u);

          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            const ir::Value::ID hflag_value = instruction->GetOut(1u);
  //          const bool n_flag = result & 0x80000000u;
  //          const bool z_flag = result == 0u;
  //          const bool c_flag = result < lhs;
            const bool v_flag = (~(lhs ^ rhs) & (result ^ lhs)) >> 31;

            u32 nzcv_value = result & 0x80000000u;
            if(result == 0u) nzcv_value |= 0x40000000u;
            if(result < lhs) nzcv_value |= 0x20000000u;
            if(v_flag) nzcv_value |= 0x10000000u;

            m_host_regs[hflag_value].data_u32 = nzcv_value;
          }

          m_host_regs[result_value].data_u32 = result;
          break;
        }
        case InstructionType::ADC: {
          const u32 lhs = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          const u32 rhs = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          const u32 carry = (m_host_regs[instruction->GetArg(2).AsValue()].data_u32 >> 29) & 1u;
          const u64 result64 = (u64)lhs + (u64)rhs + (u64)carry;

          const ir::Value::ID result_value = instruction->GetOut(0u);

          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            const ir::Value::ID hflag_value = instruction->GetOut(1u);
            const bool v_flag = (~(lhs ^ rhs) & (result64 ^ lhs)) >> 31;

            u32 nzcv_value = result64 & 0x80000000u;
            if((u32)result64 == 0u) nzcv_value |= 0x40000000u;
            if(result64 & 0x100000000ull) nzcv_value |= 0x20000000u;
            if(v_flag) nzcv_value |= 0x10000000u;

            m_host_regs[hflag_value].data_u32 = nzcv_value;
          }

          m_host_regs[result_value].data_u32 = (u32)result64;
          break;
        }
        case InstructionType::SUB: {
          const u32 lhs = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          const u32 rhs = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          const u32 result = lhs - rhs;

          const ir::Value::ID result_value = instruction->GetOut(0u);

          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            const ir::Value::ID hflag_value = instruction->GetOut(1u);
            const bool v_flag = ((lhs ^ rhs) & (result ^ lhs)) >> 31;

            u32 nzcv_value = result & 0x80000000u;
            if(result == 0u) nzcv_value |= 0x40000000u;
            if(lhs >= rhs) nzcv_value |= 0x20000000u;
            if(v_flag) nzcv_value |= 0x10000000u;

            m_host_regs[hflag_value].data_u32 = nzcv_value;
          }

          m_host_regs[result_value].data_u32 = result;
          break;
        }
        case InstructionType::SBC: {
          const u32 lhs = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          const u32 rhs = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          const u32 carry = ((m_host_regs[instruction->GetArg(2).AsValue()].data_u32 >> 29) & 1u) ^ 1u;
          const u32 result = lhs - rhs - carry;

          const ir::Value::ID result_value = instruction->GetOut(0u);

          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            const ir::Value::ID hflag_value = instruction->GetOut(1u);
            const bool v_flag = ((lhs ^ rhs) & (result ^ lhs)) >> 31;

            u32 nzcv_value = result & 0x80000000u;
            if(result == 0u) nzcv_value |= 0x40000000u;
            if((u64)lhs >= (u64)rhs + (u64)carry) nzcv_value |= 0x20000000u;
            if(v_flag) nzcv_value |= 0x10000000u;

            m_host_regs[hflag_value].data_u32 = nzcv_value;
          }

          m_host_regs[result_value].data_u32 = result;
          break;
        }
        case InstructionType::AND: {
          const u32 lhs = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          const u32 rhs = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          const u32 result = lhs & rhs;

          const ir::Value::ID result_value = instruction->GetOut(0u);

          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            const ir::Value::ID hflag_value = instruction->GetOut(1u);

            u32 nzcv_value = result & 0x80000000u;
            if(result == 0u) nzcv_value |= 0x40000000u;

            m_host_regs[hflag_value].data_u32 = nzcv_value;
          }

          m_host_regs[result_value].data_u32 = result;
          break;
        }
        case InstructionType::BIC: {
          const u32 lhs = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          const u32 rhs = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          const u32 result = lhs & ~rhs;

          const ir::Value::ID result_value = instruction->GetOut(0u);

          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            const ir::Value::ID hflag_value = instruction->GetOut(1u);

            u32 nzcv_value = result & 0x80000000u;
            if(result == 0u) nzcv_value |= 0x40000000u;

            m_host_regs[hflag_value].data_u32 = nzcv_value;
          }

          m_host_regs[result_value].data_u32 = result;
          break;
        }
        case InstructionType::ORR: {
          const u32 lhs = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          const u32 rhs = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          const u32 result = lhs | rhs;

          const ir::Value::ID result_value = instruction->GetOut(0u);

          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            const ir::Value::ID hflag_value = instruction->GetOut(1u);

            u32 nzcv_value = result & 0x80000000u;
            if(result == 0u) nzcv_value |= 0x40000000u;

            m_host_regs[hflag_value].data_u32 = nzcv_value;
          }

          m_host_regs[result_value].data_u32 = result;
          break;
        }
        case InstructionType::EOR: {
          const u32 lhs = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          const u32 rhs = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          const u32 result = lhs ^ rhs;

          const ir::Value::ID result_value = instruction->GetOut(0u);

          if(instruction->flags & ir::Instruction::Flag::OutputHostFlags) {
            const ir::Value::ID hflag_value = instruction->GetOut(1u);

            u32 nzcv_value = result & 0x80000000u;
            if(result == 0u) nzcv_value |= 0x40000000u;

            m_host_regs[hflag_value].data_u32 = nzcv_value;
          }

          m_host_regs[result_value].data_u32 = result;
          break;
        }
        case InstructionType::NOT: {
          m_host_regs[instruction->GetOut(0u)].data_u32 = ~m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          break;
        }

        // Other
        case InstructionType::BITCMB: {
          const u32 lhs = m_host_regs[instruction->GetArg(0u).AsValue()].data_u32;
          const u32 rhs = m_host_regs[instruction->GetArg(1u).AsValue()].data_u32;
          const u32 mask = m_host_regs[instruction->GetArg(2u).AsValue()].data_u32;
          const ir::Value::ID result_value = instruction->GetOut(0u);
          m_host_regs[result_value].data_u32 = lhs & ~mask | rhs & mask;
          break;
        }

        default: {
          ATOM_PANIC("unhandled IR instruction type: {}", (int)instruction->type);
        }
      }

      instruction = instruction->next;
    }
  }
}

static bool EvaluateCondition(ir::Condition condition, u32 flags) {
  const bool n = flags & 0x80000000u;
  const bool z = flags & 0x40000000u;
  const bool c = flags & 0x20000000u;
  const bool v = flags & 0x10000000u;

  switch(condition) {
    case ir::Condition::EQ: return z;
    case ir::Condition::NE: return !z;
    case ir::Condition::CS: return  c;
    case ir::Condition::CC: return !c;
    case ir::Condition::MI: return  n;
    case ir::Condition::PL: return !n;
    case ir::Condition::VS: return  v;
    case ir::Condition::VC: return !v;
    case ir::Condition::HI: return  c && !z;
    case ir::Condition::LS: return !c ||  z;
    case ir::Condition::GE: return n == v;
    case ir::Condition::LT: return n != v;
    case ir::Condition::GT: return !(z || (n != v));
    case ir::Condition::LE: return  (z || (n != v));
    default: ATOM_PANIC("unknown condition code: {}", (int)condition);
  }
}

} // namespace dual::arm::jit

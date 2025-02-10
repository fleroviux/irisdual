
#include <vector>

#include "host_flag_propagation_pass.hpp"

namespace dual::arm::jit::ir {

static u32 GetConditionFlagDeps(Condition condition);

void HostFlagPropagationPass::Run(Function& function) {
  for(BasicBlock* basic_block : function.basic_blocks) {
    RunBasicBlock(*basic_block);
  }
}

void HostFlagPropagationPass::RunBasicBlock(BasicBlock& basic_block) {
  std::vector<FlagState> flag_state_map{};
  flag_state_map.resize(basic_block.values.size());

  for(auto& flag_state : flag_state_map) {
    flag_state.fill(Value::invalid_id);
  }

  Instruction* instruction = basic_block.head;

  while(instruction != nullptr) {
    switch(instruction->type) {
      case Instruction::Type::CVT_HFLAG_NZCV: {
        // This instruction converts a host flags value to a NZCV (u32) value.
        // The created NZCV value has the same flag state as the host flags value.
        // That means that each flag either comes from the host flags value itself,
        // or another host flags value, which this host flags value was constructed from.
        const Value::ID hflag_value = instruction->GetArg(0u).AsValue();
        const Value::ID nzcv_value = instruction->GetOut(0u);
        for(const int flag : {0, 1, 2, 3}) {
          const Value::ID propagated_flag = flag_state_map[hflag_value][flag];
          if(propagated_flag != Value::invalid_id) {
            flag_state_map[nzcv_value][flag] = propagated_flag;
          } else {
            flag_state_map[nzcv_value][flag] = hflag_value;
          }
        }
        break;
      }
      case Instruction::Type::CVT_NZCV_HFLAG: {
        // This instruction converts a NZCV (u32) value to a host flags value.
        // The created host flags value has the same flag state as the NZCV value,
        // i.e. its flags come from the same host flags value as the NZCV value.
        const Value::ID nzcv_value = instruction->GetArg(0u).AsValue();
        const Value::ID hflag_value = instruction->GetOut(0u);
        flag_state_map[hflag_value] = flag_state_map[nzcv_value];
        break;
      }
      case Instruction::Type::BR_IF: {
        // The outcome of BR_IF depends on its condition and input host flags.
        // The condition evaluation might only use a subset of flags from the host flags value.
        // We figure out which flags the BR_IF depends on and then check if all relevant flags
        // come from the same original hflags value. If that is the case, we rewrite the BR_IF instruction to
        // use the original hflags value.
        const u32 flags_used = GetConditionFlagDeps(instruction->GetArg(0u).AsCondition());
        const Value::ID hflag_value = instruction->GetArg(1u).AsValue();
        const FlagState& flag_state = flag_state_map[hflag_value];

        Value::ID propagated_hflag_value{Value::invalid_id};
//        bool can_propagate{true};

        u32 flag_bit = 0x80000000ul;

        for(const int flag : {0, 1, 2, 3}) {
          if(flags_used & flag_bit) {
            if(propagated_hflag_value == Value::invalid_id) {
              // This is the first used flag that we can trace back to another host flags value.
              // We'll check if all other used flags can be traced back to that same host flags value.
              propagated_hflag_value = flag_state[flag];
            } else if(propagated_hflag_value != flag_state[flag]) {
              // At least one of the used flags has been traced back to another host flags value.
              // This means that we unfortunately can't rewrite the BR_IF instruction.
              propagated_hflag_value = Value::invalid_id;
              break;
            }
//            if(flag_state[flag] != propagated_hflag_value) {
//              if(propagated_hflag_value == Value::invalid_id) {
//                propagated_hflag_value = flag_state[flag];
//              } else {
//                propagated_hflag_value = Value::invalid_id;
//                break;
//              }
//            }
          }
          flag_bit >>= 1;
        }

        if(propagated_hflag_value != Value::invalid_id) {
          RewriteValueUseRefs(basic_block, hflag_value, propagated_hflag_value);
        }
        break;
      }
      case Instruction::Type::BITCMB: {
        // BITCMB is used to merge flags from two NZCV values (for example: old CPSR value and new NZ flags) into a new NZCV value.
        // The flag state of the resulting NZCV value is a combination of the flag states of both input NZCV values, based on the mask provided to BITCMB.
        const Value::ID mask_value = instruction->GetArg(2u).AsValue();
        const std::optional<u32> mask_const_maybe = TryGetConst(basic_block, mask_value);

        if(mask_const_maybe.has_value()) {
          const u32 mask = mask_const_maybe.value();

          const FlagState& lhs_flag_state = flag_state_map[instruction->GetArg(0u).AsValue()];
          const FlagState& rhs_flag_state = flag_state_map[instruction->GetArg(1u).AsValue()];
          FlagState& result_flag_state = flag_state_map[instruction->GetOut(0u)];

          u32 flag_bit = 0x80000000ul;

          for(const int flag : {0, 1, 2, 3}) {
            result_flag_state[flag] = mask & flag_bit ? rhs_flag_state[flag] : lhs_flag_state[flag];
            flag_bit >>= 1;
          }
        }
        break;
      }
      default: {
        break;
      }
    }

    instruction = instruction->next;
  }
}

static u32 GetConditionFlagDeps(Condition condition) {
  constexpr u32 n_flag = 0x80000000u;
  constexpr u32 z_flag = 0x40000000u;
  constexpr u32 c_flag = 0x20000000u;
  constexpr u32 v_flag = 0x10000000u;

  switch(condition) {
    case Condition::EQ: case Condition::NE: return z_flag;
    case Condition::CS: case Condition::CC: return c_flag;
    case Condition::MI: case Condition::PL: return n_flag;
    case Condition::VS: case Condition::VC: return v_flag;
    case Condition::HI: case Condition::LS: return c_flag | z_flag;
    case Condition::GE: case Condition::LT: return n_flag | v_flag;
    case Condition::GT: case Condition::LE: return z_flag | n_flag | v_flag;
    default: return 0;
  }
}

} // namespace dual::arm::jit::ir

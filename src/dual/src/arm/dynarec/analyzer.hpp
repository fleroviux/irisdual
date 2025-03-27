
#pragma once

#include <atom/integer.hpp>
#include <dual/arm/memory.hpp>
#include <vector>

#include "ir/input.hpp"

namespace dual::arm::jit {

class Analyzer {
  public:
    struct Link {
      enum class Type {
        Exit,
        UnconditionalBranch,
        ConditionalBranch
      };
      Type type;
      ir::Condition condition{};
      u32 targets[2]{};

      Link() : type{Type::Exit} {}
      explicit Link(u32 target_address) : type{Type::UnconditionalBranch}, targets{target_address, target_address} {}
      Link(ir::Condition condition, u32 target_address_true, u32 target_address_false) : type(Type::ConditionalBranch), condition{condition}, targets{target_address_true, target_address_false} {}
    };

    // TODO(fleroviux): rename to basic block?
    struct BlockOfCode {
      u32 address_lo;
      u32 address_hi;
      Link link;
    };

    explicit Analyzer(Memory& memory);

    std::vector<BlockOfCode> AnalyzeT16(u32 function_entrypoint);

  private:
    Memory& m_memory;
};

} // namespace dual::arm::jit


#pragma once

#include <atom/arena.hpp>
#include <oaknut/code_block.hpp>

#include "arm/dynarec/backend/backend.hpp"
#include "arm/dynarec/state.hpp"
#include "arm64_lowering_pass.hpp"

namespace dual::arm::jit {

class ARM64Backend : public Backend {
  public:
    explicit ARM64Backend(State& cpu_state);

    void Execute(const ir::Function& function, bool debug) override;

  private:
    State& m_cpu_state;
    oaknut::CodeBlock m_tmp_code_block{4096u};
    atom::Arena m_memory_arena{16384u};
    ARM64LoweringPass m_lowering_pass{};
};

} // namespace dual::arm::jit
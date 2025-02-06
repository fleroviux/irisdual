
#pragma once

#include <oaknut/code_block.hpp>

#include "arm/dynarec/backend/backend.hpp"
#include "arm/dynarec/state.hpp"

namespace dual::arm::jit {

class ARM64Backend : public Backend {
  public:
    explicit ARM64Backend(State& cpu_state);

    void Execute(const ir::Function& function, bool debug) override;

  private:
    void LowerToMIR(const ir::BasicBlock& basic_block);

    State& m_cpu_state;
    oaknut::CodeBlock m_tmp_code_block{4096u};
};

} // namespace dual::arm::jit
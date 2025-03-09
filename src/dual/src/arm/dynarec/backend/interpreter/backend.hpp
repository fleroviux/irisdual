
#pragma once

#include <dual/arm/memory.hpp>
#include <atom/integer.hpp>
#include <vector>

#include "arm/dynarec/backend/backend.hpp"
#include "arm/dynarec/state.hpp"

namespace dual::arm::jit {

class InterpreterBackend final : public Backend {
  public:
    explicit InterpreterBackend(State& cpu_state, Memory& memory);

    void Execute(const ir::Function& function, bool debug) override;

  private:
    union HostReg {
      u32 data_u32;
    };

    State& m_cpu_state;
    Memory& m_memory;
    std::vector<HostReg> m_host_regs{};
};

} // namespace dual::arm::jit

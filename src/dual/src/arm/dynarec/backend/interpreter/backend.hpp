
#pragma once

#include <atom/integer.hpp>
#include <vector>

#include "arm/dynarec/backend/backend.hpp"
#include "arm/dynarec/state.hpp"

namespace dual::arm::jit {

class InterpreterBackend final : public Backend {
  public:
    explicit InterpreterBackend(State& cpu_state);

    void Execute(const ir::Function& function, bool debug) override;

  private:
    union HostReg {
      u32 data_u32;
    };

    State& m_cpu_state;
    std::vector<HostReg> m_host_regs{};
};

} // namespace dual::arm::jit

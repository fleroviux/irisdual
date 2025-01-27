
#pragma once

#include "arm/dynarec/backend/backend.hpp"
#include "arm/dynarec/state.hpp"

namespace dual::arm::jit {

class ARM64Backend : public Backend {
  public:
    explicit ARM64Backend(State& cpu_state);

    void Execute(const ir::Function& function) override;

  private:
    State& m_cpu_state;
};

} // namespace dual::arm::jit
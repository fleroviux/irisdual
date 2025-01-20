
#pragma once

#include <atom/panic.hpp>
#include <dual/arm/cpu.hpp>

#include "value.hpp"

namespace dual::arm::jit::ir {

using GPR = dual::arm::CPU::GPR;

class Input {
  public:
    enum class Type {
      Null,
      Value,
      GPR
    };

    Input() : m_type{Type::Null} {}
    explicit Input(const Value& value) : m_type{Type::Value}, m_value{value.id} {}
    explicit Input(GPR gpr) : m_type{Type::GPR}, m_gpr{gpr} {}

    [[nodiscard]] Type GetType() const { return m_type; }
    [[nodiscard]] bool Is(Type type) const { return m_type == type; }

    [[nodiscard]] Value::ID AsValue() const {
      DebugCheckType(Type::Value);
      return m_value;
    }

    [[nodiscard]] GPR AsGPR() const {
      DebugCheckType(Type::GPR);
      return m_gpr;
    }

  private:
    void DebugCheckType(Type type) const {
#ifndef NDEBUG
      if(m_type != type) {
        ATOM_PANIC("bad input type: expected {} but got {}", (int)type, (int)m_type);
      }
#endif
    }

    Type m_type;
    union {
      Value::ID m_value;
      GPR m_gpr;
    };
};

} // namespace dual::arm::jit::ir

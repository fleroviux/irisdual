
#pragma once

#include <atom/panic.hpp>
#include <atom/integer.hpp>
#include <dual/arm/cpu.hpp>

#include "value.hpp"

namespace dual::arm::jit::ir {

using GPR = dual::arm::CPU::GPR;
using Mode = dual::arm::CPU::Mode;

class Input {
  public:
    enum class Type {
      Null,
      Value,
      GPR,
      Mode,
      ConstU32
    };

    Input() : m_type{Type::Null} {}
    explicit Input(const Value& value) : m_type{Type::Value}, m_value{value.id} {}
    explicit Input(GPR gpr) : m_type{Type::GPR}, m_gpr{gpr} {}
    explicit Input(Mode mode) : m_type{Type::Mode}, m_mode{mode} {}
    explicit Input(u32 const_u32) : m_type{Type::ConstU32}, m_const_u32{const_u32} {}

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

    [[nodiscard]] Mode AsMode() const {
      DebugCheckType(Type::Mode);
      return m_mode;
    }

    [[nodiscard]] u32 AsConstU32() const {
      DebugCheckType(Type::ConstU32);
      return m_const_u32;
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
      Mode m_mode;
      u32 m_const_u32;
    };
};

} // namespace dual::arm::jit::ir

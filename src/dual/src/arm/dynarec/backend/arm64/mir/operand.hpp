
#pragma once

#include <atom/integer.hpp>
#include <dual/arm/cpu.hpp>

#include "vreg.hpp"

namespace dual::arm::jit::a64mir {

using GPR = dual::arm::CPU::GPR;
using Mode = dual::arm::CPU::Mode;

class Operand {
  public:
    enum class Type {
      Null,
      VReg,
      GPR,
      Mode
    };

    Operand() : m_type{Type::Null} {}
    explicit Operand(const VReg& vreg) : m_type{Type::VReg}, m_vreg{vreg.id} {}
    explicit Operand(GPR gpr) : m_type{Type::GPR}, m_gpr{gpr} {}
    explicit Operand(Mode mode) : m_type{Type::Mode}, m_mode{mode} {}

    [[nodiscard]] Type GetType() const { return m_type; }
    [[nodiscard]] bool Is(Type type) const { return m_type == type; }

    [[nodiscard]] VReg::ID AsVReg() const {
      DebugCheckType(Type::VReg);
      return m_vreg;
    }

    [[nodiscard]] GPR AsGPR() const {
      DebugCheckType(Type::GPR);
      return m_gpr;
    }

    [[nodiscard]] Mode AsMode() const {
      DebugCheckType(Type::Mode);
      return m_mode;
    }

  private:
    void DebugCheckType(Type type) const {
#ifndef NDEBUG
      if(m_type != type) {
        ATOM_PANIC("bad operand type: expected {} but got {}", (int)type, (int)m_type);
      }
#endif
    }

    Type m_type;
    union {
      VReg::ID m_vreg;
      GPR m_gpr;
      Mode m_mode;
    };
};

} // namespace dual::arm::jit::a64mir

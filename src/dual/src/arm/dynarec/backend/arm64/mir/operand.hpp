
#pragma once

#include <atom/integer.hpp>
#include <dual/arm/cpu.hpp>

#include "address.hpp"
#include "vreg.hpp"

namespace dual::arm::jit::a64mir {

class Operand {
  public:
    enum class Type {
      Null,
      VReg,
      Address
    };

    Operand() : m_type{Type::Null} {}
    explicit Operand(Address address) : m_type{Type::Address}, m_address{address} {}
    explicit Operand(const VReg& vreg) : m_type{Type::VReg}, m_vreg{vreg.id} {}

    [[nodiscard]] Type GetType() const { return m_type; }
    [[nodiscard]] bool Is(Type type) const { return m_type == type; }

    [[nodiscard]] VReg::ID AsVReg() const {
      DebugCheckType(Type::VReg);
      return m_vreg;
    }

    [[nodiscard]] const Address& AsAddress() const {
      DebugCheckType(Type::Address);
      return m_address;
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
      Address m_address;
    };
};

} // namespace dual::arm::jit::a64mir

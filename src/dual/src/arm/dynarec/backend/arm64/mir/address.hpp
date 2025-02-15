
#pragma once

#include <atom/integer.hpp>
#include <atom/panic.hpp>
#include <oaknut/oaknut.hpp>

namespace dual::arm::jit::a64mir {

// TODO(fleroviux): replace oaknut::XReg with something more generic

class AddressOffset {
  public:
    enum class Type {
      Imm,
      Reg
    };

    struct RegOffset {
      oaknut::XReg reg;
      oaknut::IndexExt ext;
      int ext_imm;
    };

    AddressOffset() : m_type{Type::Imm}, m_imm_offset{0} {}

    explicit AddressOffset(i32 imm) : m_type{Type::Imm}, m_imm_offset{imm} {}

    explicit AddressOffset(oaknut::XReg reg, oaknut::IndexExt ext = oaknut::IndexExt::LSL, int ext_imm = 0)
        : m_type{Type::Reg}, m_reg_offset{reg, ext, ext_imm} {}

    [[nodiscard]] bool IsImm() const { return m_type == Type::Imm; }
    [[nodiscard]] bool IsReg() const { return m_type == Type::Reg; }

    [[nodiscard]] i32 GetImm() const {
      ATOM_ASSERT(IsImm(), "address offset is not imm")
      return m_imm_offset;
    }

    [[nodiscard]] RegOffset GetReg() const {
      ATOM_ASSERT(IsReg(), "address offset is not reg")
      return m_reg_offset;
    }

  private:
    Type m_type;
    union {
      i32 m_imm_offset;
      RegOffset m_reg_offset;
    };
};

struct Address {
  enum Mode {
    BasePlusOffset,
    PreIndexed,
    PostIndexed
  };

  explicit Address(oaknut::XReg reg_base, AddressOffset offset = AddressOffset{0}, Mode mode = Mode::BasePlusOffset)
      : reg_base{reg_base}
      , offset{offset}
      , mode{mode} {
  }

  oaknut::XReg reg_base;
  AddressOffset offset{};
  Mode mode{Mode::BasePlusOffset};
};

} // namespace dual::arm::jit::a64mir


#pragma once

#include <atom/integer.hpp>
#include <atom/non_copyable.hpp>
#include <limits>
#include <vector>

namespace dual::arm::jit::ir {

struct Instruction;

struct Ref {
  Instruction* instruction{};
  int slot;
};

struct Value : atom::NonCopyable {
  using ID = u16;

  static constexpr ID invalid_id = std::numeric_limits<ID>::max();

  enum class DataType : u8 {
    U32,
    HostFlags
  };

  Value(ID id, DataType data_type) : id{id}, data_type{data_type} {}

  ID id;
  DataType data_type;
  Ref create_ref;
  mutable std::vector<Ref> use_refs{};
};

template<Value::DataType data_type_>
struct TypedValue : Value {
  explicit TypedValue(ID id) : Value{id, data_type_} {}
};

using U32Value = TypedValue<Value::DataType::U32>;
using HostFlagsValue = TypedValue<Value::DataType::HostFlags>;

} // namespace dual::arm::jit::ir

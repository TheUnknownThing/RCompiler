#include "ir/context.hpp"

namespace rc::ir {

SemType Context::lookup_type(const BaseNode *node) const {
  auto it = expr_cache_.find(node);
  if (it == expr_cache_.end()) {
    throw std::runtime_error("Context::lookupType: type not found for node");
  }
  return it->second;
}
TypePtr Context::resolve_type(const SemType &type) const {
  if (type.is_primitive()) {
    switch (type.as_primitive().kind) {
    case SemPrimitiveKind::BOOL:
      return IntegerType::i1();
    case SemPrimitiveKind::I32:
      return IntegerType::i32(true);
    case SemPrimitiveKind::U32:
      return IntegerType::i32(false);
    case SemPrimitiveKind::ISIZE:
      return IntegerType::isize();
    case SemPrimitiveKind::USIZE:
      return IntegerType::usize();
    case SemPrimitiveKind::CHAR:
      return IntegerType::i8(false);
    case SemPrimitiveKind::STR:
      return IntegerType::i8(false);
    case SemPrimitiveKind::STRING:
      return std::make_shared<PointerType>(IntegerType::i8(false));
    case SemPrimitiveKind::UNIT:
      return std::make_shared<VoidType>();
    case SemPrimitiveKind::NEVER:
      return std::make_shared<VoidType>();
    case SemPrimitiveKind::ANY_INT:
      // TODO: resolve ANY_INT
      return IntegerType::i32(true);
    default:
      throw std::runtime_error(
          "Context::resolveType: unsupported primitive kind");
    }
  }
  if (type.is_reference()) {
    return std::make_shared<PointerType>(
        resolve_type(*type.as_reference().target));
  }
  if (type.is_array()) {
    return std::make_shared<ArrayType>(resolve_type(*type.as_array().element),
                                       type.as_array().size);
  }
  if (type.is_tuple()) {
    std::vector<TypePtr> elems;
    elems.reserve(type.as_tuple().elements.size());
    for (const auto &elem : type.as_tuple().elements) {
      elems.push_back(resolve_type(elem));
    }
    return std::make_shared<StructType>(std::move(elems));
  }
  if (type.is_named()) {
    const auto *item = type.as_named().item;
    if (!item)
      throw std::runtime_error(
          "Context::resolveType: named item missing metadata");
    if (item->kind == ItemKind::Struct && item->has_struct_meta()) {
      std::vector<TypePtr> fields;
      for (const auto &field : item->as_struct_meta().named_fields) {
        fields.push_back(resolve_type(field.second));
      }
      return std::make_shared<StructType>(std::move(fields), item->name);
    }
    throw std::runtime_error(
        "Context::resolveType: unsupported named item kind");
  }
  throw std::runtime_error("Context::resolveType: unsupported type kind");
}

} // namespace rc::ir

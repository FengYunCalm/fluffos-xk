#include "vm/frozen_value.h"

#include "vm/internal/base/interpret.h"

#include <cstring>
#include <unordered_set>

namespace {
constexpr int k_max_frozen_value_depth = 8;

const char *safe_error_prefix(const char *error_prefix) {
  return error_prefix && error_prefix[0] ? error_prefix : "value";
}

bool report_validation_error(const char *prefix, const char *message, std::string *error) {
  if (error) {
    *error = std::string(safe_error_prefix(prefix)) + message;
  }
  return false;
}

template <typename T>
class ActiveValueScope {
 public:
  ActiveValueScope(std::unordered_set<const T *> *active, const T *value)
      : active_(active), value_(value), entered_(active_->insert(value).second) {}

  ~ActiveValueScope() {
    if (entered_) {
      active_->erase(value_);
    }
  }

  bool entered() const { return entered_; }

 private:
  std::unordered_set<const T *> *active_;
  const T *value_;
  bool entered_;
};

struct FrozenTraversalState {
  std::unordered_set<const array_t *> active_arrays;
  std::unordered_set<const mapping_t *> active_mappings;
};

bool frozen_value_safe_impl(const svalue_t *value, int depth, int max_depth,
                            const char *error_prefix, std::string *error,
                            FrozenTraversalState *state) {
  if (!value) {
    return true;
  }
  if (depth > max_depth) {
    return report_validation_error(error_prefix, " nesting is too deep", error);
  }

  switch (value->type) {
    case T_NUMBER:
    case T_REAL:
    case T_STRING:
      return true;
    case T_ARRAY: {
      if (!value->u.arr) {
        return report_validation_error(error_prefix, " contains a null array", error);
      }
      ActiveValueScope scope(&state->active_arrays, value->u.arr);
      if (!scope.entered()) {
        return report_validation_error(error_prefix, " contains a cycle", error);
      }
      for (int i = 0; i < value->u.arr->size; i++) {
        if (!frozen_value_safe_impl(&value->u.arr->item[i], depth + 1, max_depth,
                                    error_prefix, error, state)) {
          return false;
        }
      }
      return true;
    }
    case T_MAPPING: {
      if (!value->u.map || !value->u.map->table) {
        return report_validation_error(error_prefix, " contains a null mapping", error);
      }
      ActiveValueScope scope(&state->active_mappings, value->u.map);
      if (!scope.entered()) {
        return report_validation_error(error_prefix, " contains a cycle", error);
      }
      for (unsigned int i = 0; i <= value->u.map->table_size; i++) {
        for (auto *node = value->u.map->table[i]; node; node = node->next) {
          if (node->values[0].type != T_STRING) {
            return report_validation_error(error_prefix, " mapping keys must be strings", error);
          }
          if (!frozen_value_safe_impl(&node->values[1], depth + 1, max_depth,
                                      error_prefix, error, state)) {
            return false;
          }
        }
      }
      return true;
    }
    default:
      return report_validation_error(error_prefix,
                                     " must be frozen data, not object/function/buffer/class",
                                     error);
  }
}

class ArrayAllocationGuard {
 public:
  explicit ArrayAllocationGuard(array_t *value) : value_(value) {}
  ~ArrayAllocationGuard() {
    if (value_) {
      free_array(value_);
    }
  }

  void release() { value_ = nullptr; }

 private:
  array_t *value_;
};

class MappingAllocationGuard {
 public:
  explicit MappingAllocationGuard(mapping_t *value) : value_(value) {}
  ~MappingAllocationGuard() {
    if (value_) {
      free_mapping(value_);
    }
  }

  void release() { value_ = nullptr; }

 private:
  mapping_t *value_;
};

class SvalueGuard {
 public:
  explicit SvalueGuard(const char *reason) : reason_(reason) {}
  ~SvalueGuard() {
    if (active_) {
      free_svalue(&value_, reason_);
    }
  }

  svalue_t *get() { return &value_; }
  void release() { active_ = false; }

 private:
  svalue_t value_{const0u};
  const char *reason_;
  bool active_{true};
};

struct FrozenCopyState : FrozenTraversalState {};

bool copy_frozen_svalue_impl(svalue_t *dest, const svalue_t *source, int depth,
                             FrozenCopyState *state);

bool copy_frozen_array(svalue_t *dest, const array_t *source, int depth,
                       FrozenCopyState *state) {
  if (!source || source->size < 0) {
    return false;
  }
  ActiveValueScope scope(&state->active_arrays, source);
  if (!scope.entered()) {
    return false;
  }

  auto *array = allocate_array(source->size);
  ArrayAllocationGuard array_guard(array);
  for (int i = 0; i < source->size; i++) {
    if (!copy_frozen_svalue_impl(&array->item[i], &source->item[i], depth + 1, state)) {
      return false;
    }
  }
  dest->type = T_ARRAY;
  dest->subtype = 0;
  dest->u.arr = array;
  array_guard.release();
  return true;
}

bool copy_frozen_mapping(svalue_t *dest, const mapping_t *source, int depth,
                         FrozenCopyState *state) {
  if (!source || !source->table) {
    return false;
  }
  ActiveValueScope scope(&state->active_mappings, source);
  if (!scope.entered()) {
    return false;
  }

  auto *map = allocate_mapping(MAP_COUNT(source));
  MappingAllocationGuard map_guard(map);
  for (unsigned int i = 0; i <= source->table_size; i++) {
    for (auto *node = source->table[i]; node; node = node->next) {
      if (node->values[0].type != T_STRING) {
        return false;
      }
      SvalueGuard key_guard("frozen mapping key");
      key_guard.get()->type = T_STRING;
      key_guard.get()->subtype = STRING_SHARED;
      key_guard.get()->u.string = make_shared_string(node->values[0].u.string ?
                                                      node->values[0].u.string : "");
      auto *slot = find_for_insert(map, key_guard.get(), 1);
      if (!slot) {
        return false;
      }
      free_svalue(key_guard.get(), "frozen mapping key");
      key_guard.release();
      if (!copy_frozen_svalue_impl(slot, &node->values[1], depth + 1, state)) {
        return false;
      }
    }
  }
  dest->type = T_MAPPING;
  dest->subtype = 0;
  dest->u.map = map;
  map_guard.release();
  return true;
}

bool copy_frozen_svalue_impl(svalue_t *dest, const svalue_t *source, int depth,
                             FrozenCopyState *state) {
  if (!source) {
    *dest = const0u;
    return true;
  }
  if (depth > k_max_frozen_value_depth) {
    return false;
  }

  switch (source->type) {
    case T_NUMBER:
    case T_REAL:
      *dest = *source;
      return true;
    case T_STRING:
      dest->type = T_STRING;
      dest->subtype = STRING_SHARED;
      dest->u.string = make_shared_string(source->u.string ? source->u.string : "");
      return true;
    case T_ARRAY:
      return copy_frozen_array(dest, source->u.arr, depth, state);
    case T_MAPPING:
      return copy_frozen_mapping(dest, source->u.map, depth, state);
    default:
      return false;
  }
}
}  // namespace

VMFrozenValue::~VMFrozenValue() { free_svalue(&value, "vm frozen value"); }

bool vm_copy_frozen_svalue(svalue_t *dest, svalue_t *source) {
  if (!dest) {
    return false;
  }
  try {
    FrozenCopyState state;
    return copy_frozen_svalue_impl(dest, source, 0, &state);
  } catch (...) {
    return false;
  }
}

std::shared_ptr<VMFrozenValue> vm_clone_frozen_value(svalue_t *source) {
  auto value = std::make_shared<VMFrozenValue>();
  if (!vm_copy_frozen_svalue(&value->value, source)) {
    return nullptr;
  }
  return value;
}

bool vm_frozen_value_safe_with_max_depth(const svalue_t *value, int depth, int max_depth,
                                         const char *error_prefix, std::string *error) {
  try {
    FrozenTraversalState state;
    return frozen_value_safe_impl(value, depth, max_depth, error_prefix, error, &state);
  } catch (...) {
    if (error) {
      try {
        *error = std::string(safe_error_prefix(error_prefix)) + " validation failed";
      } catch (...) {
        error->clear();
      }
    }
    return false;
  }
}

bool vm_frozen_value_safe(const svalue_t *value, int depth, const char *error_prefix,
                          std::string *error) {
  return vm_frozen_value_safe_with_max_depth(value, depth, k_max_frozen_value_depth,
                                             error_prefix, error);
}

#ifdef DEBUGMALLOC_EXTENSIONS
void vm_mark_frozen_value(const VMFrozenValue *value) {
  if (value) {
    mark_svalue(const_cast<svalue_t *>(&value->value));
  }
}

void vm_mark_frozen_value_once(const std::shared_ptr<VMFrozenValue> &value,
                               std::unordered_set<const VMFrozenValue *> &seen) {
  auto *raw = value.get();
  if (!raw || !seen.insert(raw).second) {
    return;
  }
  vm_mark_frozen_value(raw);
}
#endif

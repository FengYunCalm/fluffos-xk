/*
 * reclaim.c
 * loops through all variables in all objects looking for the possibility
 * of freeing up destructed objects (that are still hanging around because
 * of references) -- coded by Blackthorn@Genocide Feb. 1993
 */

#include "base/package_api.h"

#include "packages/core/reclaim.h"

#include <chrono>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "packages/core/call_out.h"
#include "vm/internal/base/function.h"
#include "vm/internal/base/promise.h"
#include "vm/internal/owner_runtime_coordinator.h"

namespace {

enum class ReclaimCompoundKind : uint8_t {
  kArray,
  kMapping,
  kFunction,
  kPromise,
};

struct ReclaimCompound {
  ReclaimCompoundKind kind;
  void* pointer;

  bool operator==(const ReclaimCompound& other) const {
    return kind == other.kind && pointer == other.pointer;
  }
};

struct ReclaimCompoundHash {
  size_t operator()(const ReclaimCompound& compound) const {
    auto pointer = reinterpret_cast<uintptr_t>(compound.pointer);
    return (pointer >> 4) ^ (static_cast<size_t>(compound.kind) << 1);
  }
};

struct ReclaimContext {
  std::vector<ReclaimCompound> pending;
  std::unordered_set<ReclaimCompound, ReclaimCompoundHash> visited;
  int cleaned = 0;
};

class ReclaimQuiesceScope {
 public:
  explicit ReclaimQuiesceScope(std::chrono::milliseconds timeout)
      : result_(vm_owner_recompile_quiesce_begin(timeout)) {}

  ReclaimQuiesceScope(const ReclaimQuiesceScope&) = delete;
  ReclaimQuiesceScope& operator=(const ReclaimQuiesceScope&) = delete;

  ~ReclaimQuiesceScope() {
    if (result_.ok) {
      vm_owner_recompile_quiesce_end(result_.epoch);
    }
  }

  bool ok() const {
    return result_.ok;
  }

 private:
  OwnerRecompileQuiesceResult result_;
};

void reclaim_svalue(svalue_t* value, ReclaimContext* context);

void enqueue(ReclaimCompoundKind kind, void* pointer, ReclaimContext* context) {
  if (pointer == nullptr) {
    return;
  }
  ReclaimCompound compound{kind, pointer};
  if (context->visited.insert(compound).second) {
    context->pending.push_back(compound);
  }
}

void reclaim_object_ref(object_t** object, promise_reclaim_object_role /*role*/, void* data) {
  auto* context = static_cast<ReclaimContext*>(data);
  if (object != nullptr && *object != nullptr && ((*object)->flags & O_DESTRUCTED)) {
    free_object(object, "reclaim_promise");
    *object = nullptr;
    context->cleaned++;
  }
}

void enqueue_function(funptr_t* function, void* data) {
  enqueue(ReclaimCompoundKind::kFunction, function, static_cast<ReclaimContext*>(data));
}

void reclaim_function(funptr_t* function, ReclaimContext* context) {
  if (function->hdr.owner != nullptr && (function->hdr.owner->flags & O_DESTRUCTED)) {
    /* Keep the owner/program reference pair symmetric. dealloc_funp() owns
     * the program decrement through local.prog. */
    funptr_detach_owner(function);
    context->cleaned++;
  }
  enqueue(ReclaimCompoundKind::kArray, function->hdr.args, context);
}

void reclaim_promise(promise_t* promise, ReclaimContext* context) {
  promise_reclaim_visitor_t visitor{
      .visit_svalue =
          [](svalue_t* value, void* data) {
            reclaim_svalue(value, static_cast<ReclaimContext*>(data));
          },
      .visit_object = reclaim_object_ref,
      .visit_funptr = enqueue_function,
      .data = context,
  };
  promise_visit_reclaimable_values(promise, visitor);
}

void reclaim_mapping(mapping_t* mapping, ReclaimContext* context) {
  if (mapping == nullptr) {
    return;
  }

  /* Destructed mapping keys must be unlinked rather than passed to
   * reclaim_svalue(), because changing a key in place would invalidate the
   * hash table. Other keys and all values are traversed through the same
   * iterative worklist as object variables. */
  mapping_node_t** previous;
  mapping_node_t* element;
  int bucket = mapping->table_size;
  do {
    previous = mapping->table + bucket;
    while ((element = *previous) != nullptr) {
      if (element->values[0].type == T_OBJECT && element->values[0].u.ob != nullptr &&
          (element->values[0].u.ob->flags & O_DESTRUCTED)) {
        free_object(&element->values[0].u.ob, "gc_mapping");
        element->values[0].u.ob = nullptr;
        if (!(*previous = element->next) && !mapping->table[bucket]) {
          mapping->unfilled++;
        }
        context->cleaned++;
        mapping->count--;
        total_mapping_nodes--;
        total_mapping_size -= sizeof(mapping_node_t);
        free_node(mapping, element);
        continue;
      }

      reclaim_svalue(element->values, context);
      reclaim_svalue(element->values + 1, context);
      previous = &element->next;
    }
  } while (bucket--);
}

void reclaim_svalue(svalue_t* value, ReclaimContext* context) {
  if (value == nullptr) {
    return;
  }
  switch (value->type) {
    case T_OBJECT:
      if (value->u.ob != nullptr && (value->u.ob->flags & O_DESTRUCTED)) {
        free_svalue(value, "reclaim_objects");
        *value = const0u;
        context->cleaned++;
      }
      break;
    case T_MAPPING:
      enqueue(ReclaimCompoundKind::kMapping, value->u.map, context);
      break;
    case T_ARRAY:
    case T_CLASS:
      enqueue(ReclaimCompoundKind::kArray, value->u.arr, context);
      break;
    case T_FUNCTION:
      enqueue(ReclaimCompoundKind::kFunction, value->u.fp, context);
      break;
    case T_PROMISE:
      enqueue(ReclaimCompoundKind::kPromise, value->u.prom, context);
      break;
    default:
      break;
  }
}

void drain_worklist(ReclaimContext* context) {
  while (!context->pending.empty()) {
    ReclaimCompound compound = context->pending.back();
    context->pending.pop_back();
    switch (compound.kind) {
      case ReclaimCompoundKind::kArray: {
        auto* array = static_cast<array_t*>(compound.pointer);
        for (int i = 0; i < array->size; ++i) {
          reclaim_svalue(&array->item[i], context);
        }
        break;
      }
      case ReclaimCompoundKind::kMapping:
        reclaim_mapping(static_cast<mapping_t*>(compound.pointer), context);
        break;
      case ReclaimCompoundKind::kFunction:
        reclaim_function(static_cast<funptr_t*>(compound.pointer), context);
        break;
      case ReclaimCompoundKind::kPromise:
        reclaim_promise(static_cast<promise_t*>(compound.pointer), context);
        break;
    }
  }
}

}  // namespace

int reclaim_objects(bool is_auto) {
  if (is_auto) {
    add_gametick_event(time_to_next_gametick(std::chrono::seconds(30 + random_number(30))),
                       TickEvent::callback_type([] {
                         return reclaim_objects(true);
                       }));
  }

  int64_t timeout_ms = CONFIG_INT(__RECOMPILE_OBJECT_QUIESCE_TIMEOUT_MS__);
  if (timeout_ms <= 0) {
    timeout_ms = 2000;
  }
  ReclaimQuiesceScope quiesce{std::chrono::milliseconds(timeout_ms)};
  if (!quiesce.ok()) {
    debug_message("reclaim_objects: owner runtime did not quiesce; skipping object scan\n");
    return 0;
  }

  reclaim_call_outs();
  ReclaimContext context;
  for (object_t* object = obj_list; object != nullptr; object = object->next_all) {
    if (object->prog == nullptr) {
      continue;
    }
    for (int i = 0; i < static_cast<int>(object->variables.count); ++i) {
      reclaim_svalue(&object->variables.data[i], &context);
    }
  }

  promise_reclaim_visitor_t visitor{
      .visit_svalue =
          [](svalue_t* value, void* data) {
            reclaim_svalue(value, static_cast<ReclaimContext*>(data));
          },
      .visit_object = reclaim_object_ref,
      .visit_funptr = enqueue_function,
      .data = &context,
  };
  promise_visit_reclaim_roots(visitor);
  drain_worklist(&context);

  return context.cleaned;
}

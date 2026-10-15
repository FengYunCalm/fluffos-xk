mixed deep;
mixed retained_promise;
mixed retained_reaction_source;
mixed retained_chain;
mixed retained_rejected;

void promise_callback(object target, mixed value) {
  // Keep a handler funptr argument and a chained Promise in the retained graph.
}

void build_deep(object target) {
  mixed value = target;
  for (int i = 0; i < 64; i++) {
    value = ({ value });
  }
  deep = value;
}

void build_promise(object target) {
  retained_promise = promise_create();
  promise_resolve(retained_promise, target);

  retained_reaction_source = promise_create();
  retained_chain = promise_then(retained_reaction_source,
    (: promise_callback, target :));

  retained_rejected = promise_create();
  promise_reject(retained_rejected, target);
}

async int audit_sync_catch_foreach() {
  mixed pending = promise_create();
  mixed result = catch {
    foreach (int value in ({ 1 })) {
      value += await pending;
    }
  };
  return 0;
}

async int audit_sync_catch_for() {
  mixed pending = promise_create();
  mixed result = catch {
    for (int index = 0; index < 1; index++) {
      index += await pending;
    }
  };
  return 0;
}

async int audit_sync_catch_while() {
  mixed pending = promise_create();
  mixed result = catch {
    while (0) {
      await pending;
    }
  };
  return 0;
}

async int audit_sync_catch_do() {
  mixed pending = promise_create();
  mixed result = catch {
    do {
      await pending;
    } while (0);
  };
  return 0;
}

async int audit_sync_catch_switch() {
  mixed pending = promise_create();
  mixed result = catch {
    switch (1) {
      case 1:
        await pending;
        break;
    }
  };
  return 0;
}

async int audit_sync_time_expression_while() {
  mixed pending = promise_create();
  mixed result = time_expression {
    while (0) {
      await pending;
    }
  };
  return 0;
}

async int audit_sync_catch_function_expression() {
  mixed pending = promise_create();
  mixed result = catch {
    function() { return 1; };
    return await pending;
  };
  return 0;
}

async int audit_sync_catch_acatch() {
  mixed pending = promise_create();
  mixed result = catch {
    mixed nested = acatch(await pending);
    return nested;
  };
  return 0;
}

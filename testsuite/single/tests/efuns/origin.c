private int completion_token;

void lfun() {
    ASSERT(origin() == "local");
}

void co() {
    ASSERT(origin() == "internal");
  "/command/tests"->complete_async(completion_token);
}

void ef() {
    ASSERT(origin() == "efun");
}

void do_tests() {
  completion_token = "/command/tests"->begin_async();
    ASSERT(origin() == "call_other");
    lfun();
    call_out("co", 1);
    filter( ({ 1 }), "ef");
    ASSERT(evaluate( (: origin :)) == "function pointer");
    ASSERT(evaluate( (: origin() :)) == "functional");
}

---
layout: doc
title: system / eval_cost
---
# eval_cost

### NAME

    eval_cost() - returns the evaluation cost remaining

### SYNOPSIS

    int eval_cost()

### DESCRIPTION

    eval_cost()  returns  the remaining evaluation budget. On Linux the
    budget is measured by thread CPU time; on other platforms it is measured
    by a thread-private monotonic wall-clock deadline. The budget is checked at
    VM safety points and does not interrupt a blocking native or external call.

### SEE ALSO

    catch(3),  error(3),  throw(3),  error_handler(4),   set_eval_limit(3),
    reset_eval_cost()


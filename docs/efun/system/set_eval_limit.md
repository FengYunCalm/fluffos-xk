---
layout: doc
title: system / set_eval_limit
---
# set_eval_limit

### NAME

    set_eval_limit() - set the maximum evaluation cost

### SYNOPSIS

    void set_eval_limit( int );

### DESCRIPTION

    set_eval_limit(),  with a nonzero argument, sets the maximum evaluation
    cost that is allowed for any one thread before a runtime error  occurs.
    On Linux the budget uses the thread CPU timer; on other platforms it uses
    a thread-private monotonic wall-clock deadline. Both paths are enforced at
    VM safety points and do not interrupt a blocking native or external call.
    With  a  zero argument, it sets the current evaluation counter to zero,
    and the maximum  cost  is  returned.   set_eval_limit(-1)  returns  the
    remaining evaluation cost.

### SEE ALSO

    catch(3), error(3), throw(3), error_handler(4), eval_cost(3)


---
layout: doc
title: system / reclaim_objects
---
# reclaim_objects

### NAME

    reclaim_objects - reclaim any lingering objects

### SYNOPSIS

    int reclaim_objects( void );

### DESCRIPTION

    Cycles  through  all  objects  that are loaded, and frees any lingering
    objects that it can.  This could result in a sizable amount  of  memory
    being  freed  up, depending on how the mud is coded.  Objects are typi‐
    cally left lingering when a global variable in  more  than  one  object
    contains  a  pointer  to it, and the object gets destructed.  This efun
    returns the number of destructed objects encountered in variables.

    The traversal also covers nested arrays and mappings, function values,
    and Promise-owned state, including settled results, reaction callbacks,
    chained Promises, command givers, and coroutine frames.  It uses an
    iterative worklist so deeply nested compound values do not consume the
    C++ call stack.

### SEE ALSO

    destruct(3)


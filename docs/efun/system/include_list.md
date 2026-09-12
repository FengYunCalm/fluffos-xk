---
title: system / include_list
---
# include_list

### NAME

    include_list() - list the files an object included

### SYNOPSIS

    string *include_list(object obj);

### DESCRIPTION

Returns the files that the object's program actually opened for `#include`,
including nested includes. Results use a single leading slash, preserve
first-seen order, and omit duplicate paths and the object's own source file.
An include in a disabled preprocessor branch is not reported.

If `obj` is omitted, the efun uses `this_object()`.

### SEE ALSO

    inherit_list(3), deep_inherit_list(3), recompile_object(3)

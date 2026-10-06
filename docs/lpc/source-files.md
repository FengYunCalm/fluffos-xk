---
title: Source files and object names
---
# Source files and object names

LPC source files use the **`.lpc`** extension. The legacy **`.c`**
extension remains supported. The driver separates source spelling from
loaded-object identity.

## Resolution rules

When the driver loads a file through `load_object()`, `inherit`,
`clone_object()` of an unloaded name, or the master/simul_efun setting, it
uses these rules:

* **An explicit extension is exact.** `load_object("/foo.c")` probes only
  `foo.c`; `load_object("/foo.lpc")` probes only `foo.lpc`.
* **An extension-less name prefers `.lpc`, then falls back to `.c`.**
* If no source file matches, the master's `compile_object()` hook receives
  the extension-less name.

## Object names and program names

Loaded object names are extension-less. Loading `/std/room.lpc` produces the
object `/std/room`, so `file_name()`, `base_name()`, and the object registry
are extension-blind:

```c
object ob = load_object("/std/room");
file_name(ob);                             // "/std/room"
find_object("/std/room.c") == ob;         // true after loading
```

The program name is different. `prog->filename` retains the actual source
file, including `.lpc` or `.c`. `inherit_list()`, `deep_inherit_list()`,
include tracking, error diagnostics, and save-object headers use that actual
source spelling.

Use extension-less names in normal mudlib code. Use an explicit extension only
when selecting a specific source file. The complete resolution behavior is
covered by `testsuite/single/tests/efuns/dual_extension.lpc`.

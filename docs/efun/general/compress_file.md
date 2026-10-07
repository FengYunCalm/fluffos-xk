---
layout: doc
title: general / compress_file
---
# compress_file

### SYNOPSIS

```lpc
int compress_file(string source, string destination);
int compress_file(string source);
```

Compresses `source` into gzip output. Without `destination`, appends `.gz` to the
source name; an already suffixed source returns 0 in this form. An explicit
destination may overwrite an existing file.

Returns 1 only after reading, writing, closing both streams, and deleting the
source succeed. Returns 0 on failure. Read, write, or close failures retain the
source; failure to delete the source returns 0 even if output is complete.

Source and destination referring to the same regular file, including hard-link
and symbolic-link aliases, are rejected before truncation. Other symbolic links
retain the existing follow-link behavior.

This operation is not an atomic replacement or a durability guarantee. A failed
operation may leave empty or partial output, including changes to an existing
destination. Do not concurrently replace or rename either path.

### SEE ALSO

[uncompress_file](uncompress_file.md), [write_file](../filesystem/write_file.md)

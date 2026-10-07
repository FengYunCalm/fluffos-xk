---
layout: doc
title: general / uncompress_file
---
# uncompress_file

### SYNOPSIS

```lpc
int uncompress_file(string source, string destination);
int uncompress_file(string source);
```

Reads `source` through zlib and writes the decompressed bytes to `destination`.
Without `destination`, removes the final `.gz` suffix; a name without that suffix
returns 0 in this form. An explicit destination may overwrite an existing file.
The existing zlib transparent-read behavior for non-gzip input is retained.

Returns 1 only after reading, writing, closing both streams, and deleting the
source succeed. Read or close errors reported by zlib, including bad CRC and
detected truncation, and output failures return 0 and retain the source.
Failure to delete the source also returns
0, even if output is complete.

Source and destination referring to the same regular file, including hard-link
and symbolic-link aliases, are rejected before truncation. Other symbolic links
retain the existing follow-link behavior. Permission callbacks retain the legacy
`compress_file` operation name for both functions.

This operation is not an atomic replacement or a durability guarantee. A failed
operation may leave empty or partial output, including changes to an existing
destination. Do not concurrently replace or rename either path.

### SEE ALSO

[compress_file](compress_file.md), [write_file](../filesystem/write_file.md)

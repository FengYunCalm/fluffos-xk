---
layout: doc
title: objects / save_object
---
# save_object

### NAME

    save_object() - save the values of variables in an object into a file

### SYNOPSIS

    int save_object( string name, int flag );

### DESCRIPTION

    Save  all  values  of  non-static  variables in this object in the file
    'name'.  valid_write() in the master object determines whether this  is
    allowed.   The  optional  second argument is a bitfield: If bit 0 is 1,
    then variables that  are  zero  (0)  are  also  saved  (normally,  they
    aren't).   Object  variables always save as 0.  If bit 1 is 1, then the
    save file will be compressed.

### RETURN VALUE

    A file save returns a positive number for success (not necessarily 1),
    or 0 for write, close, or rename failure. Permission, open, header-write,
    and serialization errors raise an LPC error.

    Output is written to the existing temporary-file path before rename.
    Failed writes or serialization skip the final rename and attempt to
    remove the temporary file after closing it. Compressed saves
    support records larger than zlib's printf buffer without changing the
    saved-value format. This does not guarantee protection against aliased
    temporary paths, concurrent path replacement, or power loss.

### SEE ALSO

    restore_object(3)


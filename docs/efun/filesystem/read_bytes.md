---
layout: doc
title: filesystem / read_bytes
---
# read_bytes

### NAME

    read_bytes()  -  reads  a contiguous series of bytes from a file into a
    string

### SYNOPSIS

    string read_bytes( string path, int start, int length );

### DESCRIPTION

    This function reads 'length' bytes beginning at byte # 'start'  in  the
    file named 'path'. The bytes are returned as a string. A request that
    extends past EOF is limited to the remaining bytes; an offset at or past
    EOF returns 0. If the second and third arguments are omitted, the entire
    file is requested, subject to the configured transfer limit.

    Metadata, read, stream, or close errors return 0, not partial data from
    a failed operation. Transfer-limit violations retain their LPC error.

### SEE ALSO

    read_file(3), write_bytes(3)


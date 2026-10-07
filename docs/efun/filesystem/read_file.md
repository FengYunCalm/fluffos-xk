---
layout: doc
title: filesystem / read_file
---
# read_file

### NAME

    read_file() - read a file into a string

### SYNOPSIS

    string read_file( string file, int start_line, int number_of_lines );

### DESCRIPTION

    Read  a  line  of text from a file into a string.  The second and third
    arguments are optional.  If only the first argument is  specified,  the
    file is read up to the configured read-file limit.

    The  start_line  is the line number of the line you wish to read.  This
    routine returns 0 if the requested line cannot be found. Read errors
    and close errors, including gzip truncation detected during this read,
    return 0 rather than data from a failed operation.

### SEE ALSO

    file_size(3), read_buffer(3), write_file(3), async_read(3), async_write(3),
    valid_read(4), valid_write(4)


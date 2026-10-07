---
layout: doc
title: contrib / file_length
---
# file_length

### NAME

    file_length - return the line count of a file

### SYNOPSIS

    int file_length(string);

### DESCRIPTION

    returns

    - newline count; an unterminated final line does not add one
    - -1 in case of error (including read, stream, and close errors)
    - -2 if file is directory

### SEE ALSO

    file_size(3)

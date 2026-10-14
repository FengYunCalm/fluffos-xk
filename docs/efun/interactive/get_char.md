---
layout: doc
title: interactive / get_char
---
# get_char

### NAME

    get_char  -  causes  next  character of input to be sent to a specified
    function

### SYNOPSIS

    varargs void get_char( string | function fun, int flag, ... );

### DESCRIPTION

    Enable next character of user input to be sent to the function 'fun' as
    an argument. The input character will not be parsed by the driver.

    Note  that get_char is non-blocking which means that the object calling
    get_char does not pause waiting for input.  Instead the object  contin‐
    ues  to  execute  any statements following the get_char.  The specified
    function 'fun' will not be called until the user input  has  been  col‐
    lected.

    If  "get_char()"  is  called more than once in the same execution, only
    the first call has any effect.

    If optional argument 'flag' is non-zero, the char given by  the  player
    will not be echoed, and is not seen if snooped (this is useful for col‐
    lecting passwords). The `connections` debug log uses `<redacted>` for this
    character, while the callback still receives the original input.

    The function 'fun' will be called with the  user  input  as  its  first
    argument (a string). Any additional arguments supplied to get_char will
    be passed on to 'fun' as arguments following the user input.

### DELIVERY

    Exactly one keystroke is delivered per callback:

    - A printable character arrives as itself. A multi-byte UTF-8 character
      arrives as one complete valid string; a character split across input
      chunks waits for the remaining bytes.
    - Control bytes arrive verbatim, including Backspace (0x08) and Delete
      (0x7f). Character mode does not perform line editing.
    - Escape sequences arrive byte-by-byte. ESC, '[', and 'A' are three
      separate callbacks. Reassembling the sequence is the mudlib's job.
    - Malformed UTF-8 makes byte-wise progress. It does not stall waiting for
      an impossible continuation.

    Character mode is one-shot. After each callback the driver returns to line
    mode unless the callback arms get_char() again.

### BUGS

    Please note that get_char has a significant bug in MudOS 0.9  and  ear‐
    lier.  On many systems with poor telnet negotiation (read: almost every
    major workstation on the market), get_char makes screen  output  behave
    strangely.   It  is  not  recommended  for  common usage throughout the
    mudlib until that bug is fixed.  (It is currently only  known  to  work
    well for users connecting from NeXT computers.)

### SEE ALSO

    call_other(3), call_out(3), input_to(3)


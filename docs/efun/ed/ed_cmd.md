---
layout: doc
title: ed / ed_cmd
---
# ed_cmd

### NAME

    ed_cmd() - send a command to an ed session

### SYNOPSIS

    string ed_cmd(string cmd)

### DESCRIPTION

    This efun is available only if __OLD_ED__ is not defined.

    The  command  'cmd' is sent to the active ed session, and the resulting
    output is returned.

    A failed write or close reports a file write error. Save commands keep
    the changed flag and editor buffer; a failed `x` does not exit. The
    destination may already contain partial output.


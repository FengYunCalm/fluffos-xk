---
layout: doc
title: db / db_close
---
# db_close

### NAME

    db_close() - close the database connection

### SYNOPSIS

    int db_close( int handle );

### DESCRIPTION

    Closes the database connection represented by the given handle.

    Returns 1 on success, 0 otherwise. Closing a handle invalidates the
    current connection generation. An asynchronous database request that was
    queued for this handle is rejected if the handle is closed and reused
    before execution; it cannot operate on the replacement connection. The
    backend close path releases any owned result before closing the connection
    and tolerates a partially initialized connection during failure cleanup.

### SEE ALSO

    db_connect(3), db_status(3), valid_database(4)


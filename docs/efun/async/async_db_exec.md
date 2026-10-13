---
layout: doc
title: async / async_db_exec
---
# async_db_exec

### NAME

    async_db_exec() - executes an sql statement then executes a callback

### SYNOPSIS

    void async_db_exec( int handle, string sql_query, function callback );

### DESCRIPTION

    This function will execute the passed sql statement for the given data‐
    base handle.

    The integer handle is captured together with an internal connection
    generation when the request is admitted. If the handle was closed and
    reused before the worker starts, the request is rejected with the error
    string `stale database connection`; it is never executed on the new
    connection. The callback receives that string as its single argument.

    Returns the database handle to the callback function provided.

    The callback should follow this format:

    ```c
    async_db_exec(
        handle,
        sql_query, 
        function (int rows) { // number of matched rows
            mixed *results = db_fetch(handle, 1);
            db_close(handle);
        }
    );
    ```

### SEE ALSO

    db_commit(3), db_exec(3), db_fetch(3), db_rollback(3), valid_database(4)


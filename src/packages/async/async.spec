void async_read(string, function);
void async_write(string, string, int, function);
void async_getdir(string, function);

/* Promise-returning companions keep callback efuns ABI-compatible while
 * making the same worker completions usable with async/await. */
mixed async_read_promise(string);
mixed async_write_promise(string, string, int);
mixed async_getdir_promise(string);
#ifdef PACKAGE_DB
void async_db_exec(int, string, string | function, ...);
mixed async_db_exec_promise(int, string);
#endif

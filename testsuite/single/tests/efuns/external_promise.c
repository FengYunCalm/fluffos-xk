#ifdef __PACKAGE_EXTERNAL__

/* The C++ driver test invokes one of these two methods for the host platform.
 * Keeping the command selection in the test avoids asking LPC source to guess
 * the host path while exercising both handle and promise-form APIs. */
async mixed run_handle_posix() {
    int handle = external_create(3, ({"fluffos-external-promise"}));
    mixed *result = await external_run(handle);
    string output = external_stdout(handle);
    int exit_code = external_exit_code(handle);
    external_close(handle);
    return ({ result, output, exit_code });
}

async mixed run_handle_windows() {
    int handle = external_create(4, ({"/c", "echo", "fluffos-external-promise"}));
    mixed *result = await external_run(handle);
    string output = external_stdout(handle);
    int exit_code = external_exit_code(handle);
    external_close(handle);
    return ({ result, output, exit_code });
}

async mixed run_start_posix() {
    return await external_start(3, ({"fluffos-external-promise"}));
}

async mixed run_start_windows() {
    return await external_start(4, ({"/c", "echo", "fluffos-external-promise"}));
}

async mixed run_long_posix() {
    int handle = external_create(5, ({"10"}));
    return await external_run(handle);
}

async mixed run_cat_posix() {
    int handle = external_create(6, ({}));
    ASSERT_EQ(1, external_write(handle, "fluffos-external-stdin"));
    external_close_stdin(handle);
    return await external_run(handle);
}

#endif

void do_tests() {
    /* The platform-specific methods are driven by src/tests/test_lpc.cc. */
}

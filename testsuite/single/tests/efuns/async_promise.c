#ifdef __PACKAGE_ASYNC__
nosave int completed;
nosave int failed;

async mixed run_promise_forms() {
    string path = "/log/async_promise_phase3.txt";
    rm(path);

    ASSERT_EQ(0, await async_write_promise(path, "promise async payload", 1));
    ASSERT_EQ("promise async payload", await async_read_promise(path));

    string *entries = await async_getdir_promise("/log");
    ASSERT_EQ(1, member_array("async_promise_phase3.txt", entries) >= 0);

    mixed read_error = acatch {
        await async_read_promise("/does-not-exist");
        return "not reached";
    };
    ASSERT_EQ(-1, read_error);

    mixed write_error = acatch {
        await async_write_promise("/log", "not a file", 1);
        return "not reached";
    };
    ASSERT_EQ(-1, write_error);

    rm(path);
    completed = 1;
    return 0;
}
#endif

void do_tests() {
    completed = 0;
    failed = 0;
#ifndef __PACKAGE_ASYNC__
    write("PACKAGE_ASYNC is not enabled, skipping async promise tests...\n");
    return;
#else
    promise_catch(run_promise_forms(), function(mixed reason) {
        failed = 1;
    });
    call_out(function() {
        ASSERT_EQ(1, completed);
        ASSERT_EQ(0, failed);
    }, 2);
#endif
}

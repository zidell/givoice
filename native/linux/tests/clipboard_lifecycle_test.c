#define CLIPBOARD_TIMEOUT_MS 30
#include "../src/portal.c"
#include <dirent.h>
static int fd_count(void) {
    DIR *d = opendir("/proc/self/fd");
    int n = 0;
    while (readdir(d)) ++n;
    closedir(d);
    return n;
}
static void drain(GPtrArray *pending) {
    gint64 deadline = g_get_monotonic_time() + 3 * G_USEC_PER_SEC;
    while (pending->len && g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        g_usleep(1000);
    }
    g_assert_cmpuint(pending->len, ==, 0);
}
int main(void) {
    g_autoptr(GTestDBus) test_bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(test_bus);
    g_autoptr(GDBusConnection) bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    Portal p = {0};
    p.clipboard_text = g_malloc0(1024 * 1024);
    memset(p.clipboard_text, 'x', 1024 * 1024 - 1);
    int baseline = fd_count();
    for (int i = 0; i < 100; ++i) {
        int fds[2];g_assert_cmpint(pipe(fds), ==, 0);
        g_assert_true(clipboard_write_start(&p, bus, "/session", i, fds[1]));
        // No reader consumes the 1MB payload: cancellation must release it.
        drain(p.clipboard_writes);close(fds[0]);
        g_assert_cmpint(fd_count(), ==, baseline);
    }
    int readers[9];
    for (int i = 0; i < 9; ++i) {
        int fds[2];g_assert_cmpint(pipe(fds), ==, 0);readers[i] = fds[0];
        g_assert_cmpint(clipboard_write_start(&p, bus, "/session", i, fds[1]), ==, i < 8);
    }
    g_assert_cmpuint(p.clipboard_writes->len, ==, 8);
    GPtrArray *pending = g_ptr_array_ref(p.clipboard_writes);
    portal_clear(&p);drain(pending);g_ptr_array_unref(pending);
    g_clear_pointer(&p.clipboard_text, g_free);
    for (int i = 0; i < 9; ++i) close(readers[i]);
    g_assert_cmpint(fd_count(), ==, baseline);
    g_dbus_connection_close_sync(bus, NULL, NULL);
    g_clear_object(&bus);g_test_dbus_down(test_bus);
    puts("PASS: 100 blocked clipboard writes return to FD baseline; concurrency capped; clear cancels pending writes");
}

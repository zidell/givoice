#include "../src/output.c"
void debug_log(const char *message) { (void)message; }
int main(void) {
    Output out = {0};
    for (int i = 0; i < 10000; ++i) {
        out.restore_attempts = 0;
        restored(NULL, FALSE, &out);
        guint previous = out.restore_timer;
        g_assert_cmpuint(previous, !=, 0);
        restored(NULL, FALSE, &out);
        g_assert_null(g_main_context_find_source_by_id(NULL, previous));
        restored(NULL, TRUE, &out);
        g_assert_cmpuint(out.restore_timer, ==, 0);
    }
    out.restore_attempts = 0;
    restored(NULL, FALSE, &out);
    guint previous = out.restore_timer;
    output_close(&out);
    g_assert_null(g_main_context_find_source_by_id(NULL, previous));
    restored(NULL, FALSE, &out);
    g_assert_cmpuint(out.restore_timer, ==, 0);
    puts("PASS: 10000 failed/successful restore cycles retain no timer; close removes pending retry");
}

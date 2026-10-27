// Exercise the production paste scheduler against a focused Wayland text field.
// No microphone, transcription request, or user text is used.
#define main givoice_app_main
#include "../src/main.c"
#undef main
static GtkWidget *target;
static guint rounds, submitted, bare_v;
static int result = 1;
static const char *sample = "한글 반복 붙여넣기 검사";
static gboolean observed(GtkWidget *widget, GdkEventKey *event, void *user) {
    (void)widget; (void)user;
    if (event->keyval == GDK_KEY_v && !(event->state & GDK_CONTROL_MASK)) bare_v++;
    return FALSE;
}
static void submitted_text(GtkEntry *entry, void *user) {
    (void)entry; (void)user; submitted++;
}
static void queue_sample(void) {
    gtk_entry_set_text(GTK_ENTRY(target), "");
    app.paste_steps = paste_segments("한글 반복 붙여넣기 검사[enter]");
    app.paste_index = app.paste_key_phase = 0;
    app.pasting_text = FALSE;
    app.state = TRANSCRIBING;
}
static gboolean tick(void *user) {
    (void)user;
    if (!gtk_window_is_active(GTK_WINDOW(app.window)) || !gtk_widget_has_focus(target)) {
        g_printerr("FAIL: private paste test lost focus; stopped injection\n");
        release_paste_keys();
        gtk_main_quit();
        return G_SOURCE_REMOVE;
    }
    if (app.paste_steps) {
        paste_tick(NULL);
    } else {
        if (!g_str_equal(gtk_entry_get_text(GTK_ENTRY(target)), sample) ||
            submitted != rounds + 1 || bare_v || app.paste_held_count) {
            g_printerr("FAIL: round=%u text_match=%d submitted=%u bare_v=%u held=%u\n",
                rounds, g_str_equal(gtk_entry_get_text(GTK_ENTRY(target)),sample),
                submitted, bare_v, app.paste_held_count);
            gtk_main_quit();
            return G_SOURCE_REMOVE;
        }
        if (++rounds == 20) {
            result = 0;
            g_print("PASS: production scheduler: 20 Korean pastes, 20 Enter events, no bare V or held keys\n");
            gtk_main_quit();
            return G_SOURCE_REMOVE;
        }
        queue_sample();
    }
    return G_SOURCE_CONTINUE;
}
static void ready(GVariant *values, const GError *error, void *user) {
    (void)values; (void)user;
    if (error) { g_printerr("Permission: %s\n", error->message);gtk_main_quit();return; }
    gtk_window_present(GTK_WINDOW(app.window));
    gtk_widget_grab_focus(target);
    queue_sample();
    g_timeout_add(150, tick, NULL);
}
static gboolean timeout_test(void *user) {
    (void)user;g_printerr("FAIL: paste test timed out\n");gtk_main_quit();return G_SOURCE_REMOVE;
}
int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    settings_init(&app.settings);
    g_free(app.settings.overlay_position);
    app.settings.overlay_position = g_strdup("hidden");
    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), "Givoice private paste test");
    target = gtk_entry_new();
    gtk_container_add(GTK_CONTAINER(app.window), target);
    app.key = gtk_entry_new(); app.model = gtk_combo_box_text_new();
    app.refresh = gtk_button_new();app.record_button = gtk_button_new();
    app.status = gtk_label_new("");app.menu_status = gtk_menu_item_new();
    app.indicator = app_indicator_new_with_path("givoice-paste-test", "audio-input-microphone", APP_INDICATOR_CATEGORY_APPLICATION_STATUS, NULL);
    g_signal_connect(target, "activate", G_CALLBACK(submitted_text), NULL);
    g_signal_connect(app.window, "key-press-event", G_CALLBACK(observed), NULL);
    g_autoptr(GError) error = NULL;
    if (!portal_init(&app.portal, NULL, NULL, &error)) return 1;
    app.portal.keycode_for_keysym = input_keycode;
    g_autofree char *config = g_build_filename(g_get_user_config_dir(),"givoice",NULL);
    app.portal.keyboard_token_path = portal_keyboard_token_path(config);
    if (!g_file_get_contents(app.portal.keyboard_token_path, &app.portal.keyboard_restore_token,NULL,NULL)) return 77;
    gtk_widget_show_all(app.window);
    g_timeout_add_seconds(45, timeout_test, NULL);
    portal_enable_keyboard(&app.portal, ready, NULL);
    gtk_main();
    release_paste_keys();portal_clear(&app.portal);
    gtk_widget_destroy(app.window);
    return result;
}

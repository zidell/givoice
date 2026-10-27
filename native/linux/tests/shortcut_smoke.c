#include <gtk/gtk.h>
#include "portal.h"
#include "shortcut_presets.h"

static Portal portal;
static GtkWidget *window, *entry;
static guint preset_index, phase, cycle, presses, releases, leaked;
static gboolean bound, failed;
static gint64 pressed_at;
static int result = 1;
static const guint modifier_codes[] = {29, 56, 42, 125};
static void event(const char *id, gboolean down, void *user) {
    (void)user;
    if (!g_str_equal(id,"record")) return;
    if (down) presses++; else releases++;
}
static void complete(GVariant *values, const GError *error, void *user) {
    (void)values; (void)user;
    if (error) { g_printerr("Bind failed: %s\n", error->message);failed=TRUE; }
    bound=TRUE;
}
static gboolean key(guint code, gboolean down) {
    g_autoptr(GError) error=NULL;
    g_autoptr(GVariant) reply=g_dbus_connection_call_sync(portal.bus,
        "com.videostew.givoice.Driver", "/com/videostew/givoice/Driver",
        "com.videostew.givoice.Driver", "Key", g_variant_new("(ub)",code,down),
        G_VARIANT_TYPE("(b)"),0,1000,NULL,&error);
    gboolean ok=FALSE;
    if (reply) g_variant_get(reply,"(b)",&ok);
    if (!ok) g_printerr("Injection stopped: %s\n",error?error->message:"private test lost focus");
    return ok;
}
static gboolean mods(gboolean down) {
    guint mask=shortcut_presets[preset_index].modifiers;
    for (guint i=0;i<G_N_ELEMENTS(modifier_codes);i++)
        if ((mask & (1u<<i)) && !key(modifier_codes[i],down)) return FALSE;
    return TRUE;
}
static gboolean received(GtkWidget *widget,GdkEventKey *event,void *user) {
    (void)widget;(void)user;
    // Modifiers used to form combinations can reach the focused app. The
    // recording key itself, its repeats, and its release must be consumed.
    if (preset_index < SHORTCUT_PRESET_COUNT &&
        event->hardware_keycode == shortcut_presets[preset_index].scan_code + 8) leaked++;
    return FALSE;
}
#define CHECK(x) do { if(!(x)) { g_printerr("FAIL: %s phase=%u cycle=%u events=%u/%u leaked=%u: %s\n",shortcut_presets[preset_index].trigger,phase,cycle,presses,releases,leaked,#x);gtk_main_quit();return G_SOURCE_REMOVE;} } while(0)
static gboolean tick(void *user) {
    (void)user;
    CHECK(!failed);
    const ShortcutPreset *preset=&shortcut_presets[preset_index];
    switch(phase) {
    case 0:
        bound=FALSE;presses=releases=leaked=cycle=0;
        g_free(portal.preferred_trigger);portal.preferred_trigger=g_strdup(preset->trigger);
        portal_bind(&portal,complete,NULL);break;
    case 1: CHECK(bound && (portal.native_shortcut || portal.shortcuts));break;
    case 2: CHECK(mods(TRUE));CHECK(key(preset->scan_code,TRUE));pressed_at=g_get_monotonic_time();break;
    case 3:
        CHECK(presses==cycle+1 && releases==cycle);
        // Repeat must not toggle recording. Also exercise long holds (>1s).
        CHECK(key(preset->scan_code,TRUE));
        if (!cycle && g_get_monotonic_time()-pressed_at<1200000) return G_SOURCE_CONTINUE;
        break;
    case 4: CHECK(mods(FALSE));break; // Release modifiers before the recording key.
    case 5: CHECK(key(preset->scan_code,FALSE));break;
    case 6:
        CHECK(presses==cycle+1 && releases==cycle+1 && leaked==0);
        if (++cycle<3) { phase=2;return G_SOURCE_CONTINUE; }
        g_print("PASS: %s: bind, hold, repeated presses, taps, modifiers-first release, swallowed key\n",preset->trigger);
        portal_pause_shortcuts(&portal);break;
    case 7:
        CHECK(!portal.native_shortcut);
        if (!preset->modifiers) {
            CHECK(key(preset->scan_code,TRUE));CHECK(key(preset->scan_code,FALSE));
            break;
        }
        phase=13;return G_SOURCE_CONTINUE;
    case 8:
        CHECK(presses==3 && releases==3);
        bound=FALSE;portal_bind(&portal,complete,NULL);break;
    case 9: CHECK(bound && portal.native_shortcut);CHECK(key(preset->scan_code,TRUE));break;
    case 10: CHECK(presses==4 && releases==3);portal_pause_shortcuts(&portal);break;
    case 11: CHECK(key(preset->scan_code,FALSE));break;
    case 12:
        CHECK(presses==4 && releases==4);
        g_print("PASS: %s: paused capture, rebind, disable while held, release drained\n",preset->trigger);break;
    case 13:
        if (++preset_index<SHORTCUT_PRESET_COUNT) { phase=0;return G_SOURCE_CONTINUE; }
        result=0;gtk_main_quit();return G_SOURCE_REMOVE;
    }
    phase++;return G_SOURCE_CONTINUE;
}
static gboolean timeout_test(void *user) {
    (void)user;g_printerr("FAIL: shortcut test timed out\n");gtk_main_quit();return G_SOURCE_REMOVE;
}
int main(int argc,char **argv) {
    gtk_init(&argc,&argv);
    g_autoptr(GError) error=NULL;
    if (!portal_init(&portal,event,NULL,&error)) return 1;
    guint owner=g_bus_own_name_on_connection(portal.bus,"com.videostew.givoice",0,NULL,NULL,NULL,NULL);
    window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window),"Private Escape test");
    entry=gtk_entry_new();gtk_container_add(GTK_CONTAINER(window),entry);
    g_signal_connect(window,"key-press-event",G_CALLBACK(received),NULL);
    g_signal_connect(window,"key-release-event",G_CALLBACK(received),NULL);
    gtk_widget_show_all(window);gtk_window_present(GTK_WINDOW(window));gtk_widget_grab_focus(entry);
    g_timeout_add(100,tick,NULL);g_timeout_add_seconds(50,timeout_test,NULL);
    gtk_main();portal_clear(&portal);g_bus_unown_name(owner);
    gtk_widget_destroy(window);return result;
}
